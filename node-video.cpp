#define NAPI_DISABLE_CPP_EXCEPTIONS 1

#include <node_api.h> 
#include <napi.h>

#include <assert.h>
#include <stdio.h> 
#include <stdlib.h>
#include <chrono>
#include <thread>
#include <vector>
#include <string.h>
#include <comutil.h>

#include <d3d12.h>
#include <d3d11_1.h>
#include <d3d10.h>

#include <wrl.h>
#include <wrl/client.h>
#include <wincodec.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfmediaengine.h>
#include <wincodec.h>
#include <windows.h>
#include <GL\glew.h>
#include <GL\wglew.h> 

/*

    Some example for GL/DX interop here: https://github.com/baldurk/renderdoc/issues/2952

    does it need to lock around the interop use?

*/

using Microsoft::WRL::ComPtr;

//Microsoft::WRL::ComPtr<ID3D12Device> m_d3dDevice = nullptr;

Microsoft::WRL::ComPtr<IMFDXGIDeviceManager> m_spDXGIManager;
Microsoft::WRL::ComPtr<ID3D11Device> m_spDX11Device = nullptr;;
Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_spDX11DeviceContext;

UINT mResetToken = 0;
HANDLE gl_handleD3D = nullptr;

DXGI_FORMAT m_d3dFormat = DXGI_FORMAT_R8G8B8A8_UNORM; // DXGI_FORMAT_B8G8R8A8_UNORM;

BSTR ConvertConstCharPtrToBSTR(const char* str) {
  int wlen = ::MultiByteToWideChar(CP_ACP, 0, str, -1, NULL, 0);
  if (wlen == 0) {
    return nullptr;
  }

  BSTR bstr = ::SysAllocStringLen(nullptr, wlen - 1);
  if (bstr == nullptr) {
      return nullptr;
  }

  ::MultiByteToWideChar(CP_ACP, 0, str, -1, bstr, wlen);
  return bstr;
}

struct MediaEngineNotifyCallback {
    virtual void OnMediaEngineEvent(DWORD meEvent, DWORD_PTR param1, DWORD param2) = 0;
};

struct MEEventProcessor : public IMFMediaEngineNotify {

    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) {
        if (__uuidof(IMFMediaEngineNotify) == riid) {
            *ppv = static_cast<IMFMediaEngineNotify*>(this);
        } else {
            *ppv = nullptr;
            return E_NOINTERFACE;
        }
        AddRef();
        return S_OK;
    }

    // EventNotify is called when the Media Engine sends an event.
    STDMETHODIMP EventNotify(DWORD meEvent, DWORD_PTR param1, DWORD param2) {
        if (meEvent == MF_MEDIA_ENGINE_EVENT_NOTIFYSTABLESTATE) {
            SetEvent(reinterpret_cast<HANDLE>(param1));
        } else {
            mCB->OnMediaEngineEvent(meEvent, param1, param2);
        }
        return S_OK;
    }

    // we are going to store and manage this, so don't worry about these functions
    STDMETHODIMP_(ULONG) AddRef() {return 0;}
    STDMETHODIMP_(ULONG) Release() {return 0;}

    void setCB(MediaEngineNotifyCallback* acb) {
        mCB = acb;
    }

    MediaEngineNotifyCallback* mCB = nullptr;
};


//----------------------------------------------
std::string MFEventToString(MF_MEDIA_ENGINE_EVENT aevent) {
    static std::unordered_map<MF_MEDIA_ENGINE_EVENT, std::string> sMFMessages =
    {
        { MF_MEDIA_ENGINE_EVENT_LOADSTART, "MF_MEDIA_ENGINE_EVENT_LOADSTART" },
            { MF_MEDIA_ENGINE_EVENT_PROGRESS, "MF_MEDIA_ENGINE_EVENT_PROGRESS" },
            { MF_MEDIA_ENGINE_EVENT_SUSPEND, "MF_MEDIA_ENGINE_EVENT_SUSPEND" },
            { MF_MEDIA_ENGINE_EVENT_ABORT, "MF_MEDIA_ENGINE_EVENT_ABORT" },
            { MF_MEDIA_ENGINE_EVENT_ERROR, "MF_MEDIA_ENGINE_EVENT_ERROR" },
            { MF_MEDIA_ENGINE_EVENT_EMPTIED, "MF_MEDIA_ENGINE_EVENT_EMPTIED" },
            { MF_MEDIA_ENGINE_EVENT_STALLED, "MF_MEDIA_ENGINE_EVENT_STALLED" },
            { MF_MEDIA_ENGINE_EVENT_PLAY, "MF_MEDIA_ENGINE_EVENT_PLAY" },
            { MF_MEDIA_ENGINE_EVENT_PAUSE, "MF_MEDIA_ENGINE_EVENT_PAUSE" },
            { MF_MEDIA_ENGINE_EVENT_LOADEDMETADATA, "MF_MEDIA_ENGINE_EVENT_LOADEDMETADATA" },
            { MF_MEDIA_ENGINE_EVENT_LOADEDDATA, "MF_MEDIA_ENGINE_EVENT_LOADEDDATA" },
            { MF_MEDIA_ENGINE_EVENT_WAITING, "MF_MEDIA_ENGINE_EVENT_WAITING" },
            { MF_MEDIA_ENGINE_EVENT_PLAYING, "MF_MEDIA_ENGINE_EVENT_PLAYING" },
            { MF_MEDIA_ENGINE_EVENT_CANPLAY, "MF_MEDIA_ENGINE_EVENT_CANPLAY" },
            { MF_MEDIA_ENGINE_EVENT_CANPLAYTHROUGH, "MF_MEDIA_ENGINE_EVENT_CANPLAYTHROUGH" },
            { MF_MEDIA_ENGINE_EVENT_SEEKING, "MF_MEDIA_ENGINE_EVENT_SEEKING" },
            { MF_MEDIA_ENGINE_EVENT_SEEKED, "MF_MEDIA_ENGINE_EVENT_SEEKED" },
            { MF_MEDIA_ENGINE_EVENT_TIMEUPDATE, "MF_MEDIA_ENGINE_EVENT_TIMEUPDATE" },
            { MF_MEDIA_ENGINE_EVENT_ENDED, "MF_MEDIA_ENGINE_EVENT_ENDED" },
            { MF_MEDIA_ENGINE_EVENT_RATECHANGE, "MF_MEDIA_ENGINE_EVENT_RATECHANGE" },
            { MF_MEDIA_ENGINE_EVENT_DURATIONCHANGE, "MF_MEDIA_ENGINE_EVENT_DURATIONCHANGE" },
            { MF_MEDIA_ENGINE_EVENT_VOLUMECHANGE, "MF_MEDIA_ENGINE_EVENT_VOLUMECHANGE" },
            { MF_MEDIA_ENGINE_EVENT_FORMATCHANGE, "MF_MEDIA_ENGINE_EVENT_FORMATCHANGE" },
            { MF_MEDIA_ENGINE_EVENT_PURGEQUEUEDEVENTS, "MF_MEDIA_ENGINE_EVENT_PURGEQUEUEDEVENTS" },
            { MF_MEDIA_ENGINE_EVENT_TIMELINE_MARKER, "MF_MEDIA_ENGINE_EVENT_TIMELINE_MARKER" },
            { MF_MEDIA_ENGINE_EVENT_BALANCECHANGE, "MF_MEDIA_ENGINE_EVENT_BALANCECHANGE" },
            { MF_MEDIA_ENGINE_EVENT_DOWNLOADCOMPLETE, "MF_MEDIA_ENGINE_EVENT_DOWNLOADCOMPLETE" },
            { MF_MEDIA_ENGINE_EVENT_BUFFERINGSTARTED, "MF_MEDIA_ENGINE_EVENT_BUFFERINGSTARTED" },
            { MF_MEDIA_ENGINE_EVENT_BUFFERINGENDED, "MF_MEDIA_ENGINE_EVENT_BUFFERINGENDED" },
            { MF_MEDIA_ENGINE_EVENT_FRAMESTEPCOMPLETED, "MF_MEDIA_ENGINE_EVENT_FRAMESTEPCOMPLETED" },
            { MF_MEDIA_ENGINE_EVENT_NOTIFYSTABLESTATE, "MF_MEDIA_ENGINE_EVENT_NOTIFYSTABLESTATE" },
            { MF_MEDIA_ENGINE_EVENT_FIRSTFRAMEREADY, "MF_MEDIA_ENGINE_EVENT_FIRSTFRAMEREADY" },
            { MF_MEDIA_ENGINE_EVENT_TRACKSCHANGE, "MF_MEDIA_ENGINE_EVENT_TRACKSCHANGE" },
            { MF_MEDIA_ENGINE_EVENT_OPMINFO, "MF_MEDIA_ENGINE_EVENT_OPMINFO" },
            { MF_MEDIA_ENGINE_EVENT_RESOURCELOST, "MF_MEDIA_ENGINE_EVENT_RESOURCELOST" },
            { MF_MEDIA_ENGINE_EVENT_DELAYLOADEVENT_CHANGED, "MF_MEDIA_ENGINE_EVENT_DELAYLOADEVENT_CHANGED" },
            { MF_MEDIA_ENGINE_EVENT_STREAMRENDERINGERROR, "MF_MEDIA_ENGINE_EVENT_STREAMRENDERINGERROR" },
            { MF_MEDIA_ENGINE_EVENT_SUPPORTEDRATES_CHANGED, "MF_MEDIA_ENGINE_EVENT_SUPPORTEDRATES_CHANGED" },
            { MF_MEDIA_ENGINE_EVENT_AUDIOENDPOINTCHANGE, "MF_MEDIA_ENGINE_EVENT_AUDIOENDPOINTCHANGE" }
    };

    if (sMFMessages.count(aevent) > 0) {
        return sMFMessages.at(aevent);
    }
    return std::to_string(aevent);
}

//----------------------------------------------
std::string MFErrorToString(MF_MEDIA_ENGINE_ERR aerror) {
    static std::unordered_map<MF_MEDIA_ENGINE_ERR, std::string> sMFErrorMessages =
    {
        {MF_MEDIA_ENGINE_ERR_NOERROR, "MF_MEDIA_ENGINE_ERR_NOERROR" },
        {MF_MEDIA_ENGINE_ERR_ABORTED, "MF_MEDIA_ENGINE_ERR_ABORTED"},
        {MF_MEDIA_ENGINE_ERR_NETWORK, "MF_MEDIA_ENGINE_ERR_NETWORK"},
        {MF_MEDIA_ENGINE_ERR_DECODE, "MF_MEDIA_ENGINE_ERR_DECODE"},
        {MF_MEDIA_ENGINE_ERR_SRC_NOT_SUPPORTED, "MF_MEDIA_ENGINE_ERR_SRC_NOT_SUPPORTED"},
        {MF_MEDIA_ENGINE_ERR_ENCRYPTED, "MF_MEDIA_ENGINE_ERR_ENCRYPTED"}
    };

    if (sMFErrorMessages.count(aerror) > 0) {
        return sMFErrorMessages.at(aerror);
    }

    return std::to_string(aerror);
}


// struct MediaEngineNotify : public IMFMediaEngineNotify {
//     // EventNotify is called when the Media Engine sends an event.
//     virtual STDMETHODIMP EventNotify(DWORD meEvent, DWORD_PTR param1, DWORD)
//     {
//         // if (meEvent == MF_MEDIA_ENGINE_EVENT_NOTIFYSTABLESTATE)
//         // {
//         //     SetEvent(reinterpret_cast<HANDLE>(param1));
//         // }
//         // else
//         // {
//         //     m_pCB->OnMediaEngineEvent(meEvent);
//         // }

//         return S_OK;
//     }
// };

struct Video : public Napi::ObjectWrap<Video>, public MediaEngineNotifyCallback {


    Video(const Napi::CallbackInfo& info) : Napi::ObjectWrap<Video>(info) {
		Napi::Env env = info.Env();
		Napi::Object This = info.This().As<Napi::Object>();

        // if (info.Length() > 0 && info[0].IsString()) {
        //     sender.SetSenderName(info[0].ToString().Utf8Value().c_str());
        // }


        This.Set("duration", duration);
        This.Set("width", width);
        This.Set("height", height);
        This.Set("paused", paused);
        This.Set("loop", loop);

	}

    ~Video() {
		printf("release Video\n");
        //sender.ReleaseSender();
    }

    Microsoft::WRL::ComPtr<IMFMediaEngine>      m_mediaEngine;
    Microsoft::WRL::ComPtr<IMFMediaEngineEx>    m_engineEx;
    MEEventProcessor mEventProcessor;

    bool loaded = false;
    double seconds = 0.0, duration = 0.0;
    bool paused = false;
    uint32_t width = 0, height = 0;
    bool loop = true;

    // the DXGL texture:
    ComPtr<ID3D11Texture2D> mDXTex{ nullptr };
    ComPtr<ID3D11Texture2D> stagingTexture{ nullptr };
    HANDLE mGLDX_Handle = nullptr;
    bool mBLocked = false;
    bool hasTexture = false;

    uint32_t gl_texid = 0;

    Napi::Value load(const Napi::CallbackInfo& info) {
		Napi::Env env = info.Env();
		Napi::Object This = info.This().As<Napi::Object>();
        if (info.Length() < 1 || !info[0].IsString()) return This;
        const char * filename = info[0].ToString().Utf8Value().c_str();
        printf("loading %s\n", filename);
        BSTR bfilename = ConvertConstCharPtrToBSTR(filename);
        loaded = false;
           
        HRESULT hr = S_OK;

        if (!m_spDX11Device) {
            if (FAILED( MFStartup ( MF_VERSION ) )) {
                fprintf(stderr, "error MFStartup\n");
                return This;
            }

            static const D3D_FEATURE_LEVEL levels[] = {
                D3D_FEATURE_LEVEL_11_1,
                D3D_FEATURE_LEVEL_11_0,
            };

            D3D_FEATURE_LEVEL FeatureLevel;

            // hr = device->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &featLevels, sizeof(featLevels));
            // D3D_FEATURE_LEVEL fLevel = D3D_FEATURE_LEVEL_11_0;
            // if (SUCCEEDED(hr))
            // {
            //     fLevel = featLevels.MaxSupportedFeatureLevel;
            // }

            hr = D3D11CreateDevice(
                nullptr,
                D3D_DRIVER_TYPE_HARDWARE,
                nullptr,
                D3D11_CREATE_DEVICE_VIDEO_SUPPORT | D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                levels,
                ARRAYSIZE(levels),
                D3D11_SDK_VERSION,
                &m_spDX11Device,
                &FeatureLevel,
                &m_spDX11DeviceContext
            );
            if (FAILED(hr)) {
                fprintf(stderr, "error D3D11CreateDevice\n");
                return This;
            }

            ComPtr<ID3D10Multithread> spMultithread;
            if (FAILED(m_spDX11Device.As(&spMultithread))) {
                fprintf(stderr, "error m_spDX11Device.As(&multithreaded)\n");
                return This;
            }
            spMultithread->SetMultithreadProtected(TRUE);

            // if (FAILED(m_spDX11Device.As(&m_spDX11Device))) {
            //     fprintf(stderr, "error m_spDX11Device.As(&m_spDX11Device)\n");
            //     return This;
            // }


            if (FAILED(MFCreateDXGIDeviceManager(&mResetToken, m_spDXGIManager.GetAddressOf()))) {
                 fprintf(stderr, "error MFCreateDXGIDeviceManager\n");
                return This;
            }
            if (FAILED((m_spDXGIManager->ResetDevice(m_spDX11Device.Get(), mResetToken)))) {
                 fprintf(stderr, "error ResetDevice\n");
                return This;
            }

            gl_handleD3D = wglDXOpenDeviceNV(m_spDX11Device.Get());
            if (gl_handleD3D == nullptr) {
                fprintf(stderr, "error wglDXOpenDeviceNV\n");
                return This;
            }

            mEventProcessor.setCB(this);

            ComPtr<IMFAttributes> attributes;
            if (FAILED(MFCreateAttributes(attributes.GetAddressOf(), 1))) {
                fprintf(stderr, "error MFCreateAttributes\n");
                return This;
            }
            if (FAILED(attributes->SetUnknown(MF_MEDIA_ENGINE_DXGI_MANAGER, reinterpret_cast<IUnknown*>(m_spDXGIManager.Get())))) {
                fprintf(stderr, "error MF_MEDIA_ENGINE_DXGI_MANAGER\n");
                return This;
            }
            if (FAILED(attributes->SetUnknown(MF_MEDIA_ENGINE_CALLBACK, reinterpret_cast<IUnknown*>(&mEventProcessor)))) {
                fprintf(stderr, "error MF_MEDIA_ENGINE_CALLBACK\n");
                return This;
            }

            if (FAILED(attributes->SetUINT32(MF_MEDIA_ENGINE_VIDEO_OUTPUT_FORMAT, m_d3dFormat))) {
                fprintf(stderr, "error MF_MEDIA_ENGINE_VIDEO_OUTPUT_FORMAT\n");
                return This;
            }

            ComPtr<IMFMediaEngineClassFactory> mfFactory;
            if (FAILED(CoCreateInstance(CLSID_MFMediaEngineClassFactory,
                nullptr,
                CLSCTX_INPROC_SERVER, //CLSCTX_ALL, //
                IID_PPV_ARGS(mfFactory.GetAddressOf())))) {
                fprintf(stderr, "error CoCreateInstance\n");
                return This;
            }

            //const DWORD flags = MF_MEDIA_ENGINE_REAL_TIME_MODE; // or 0
            const DWORD flags = MF_MEDIA_ENGINE_WAITFORSTABLE_STATE;
            if (FAILED(mfFactory->CreateInstance(
                flags,
                attributes.Get(),
                &m_mediaEngine //m_mediaEngine.ReleaseAndGetAddressOf()
                ))) {
                fprintf(stderr, "error CreateInstance\n");
                return This;
            }

            if (FAILED(m_mediaEngine.As(&m_engineEx))) {
                fprintf(stderr, "error m_mediaEngine.As(&m_engineEx)\n");
                return This;
            }


            printf("initialized\n");
        }

        m_mediaEngine->SetAutoPlay(true);
        m_mediaEngine->SetLoop(This.Get("loop").ToBoolean());

        if (FAILED(m_mediaEngine->SetSource(bfilename))) {
            fprintf(stderr, "error m_mediaEngine->SetSource\n");
            return This;
        }
        if (FAILED(m_mediaEngine->Load())) {
            fprintf(stderr, "error m_mediaEngine->SetSource\n");
            return This;
        }

        

        printf("loaded and ready\n");

        return This;
    }

    Napi::Value update(const Napi::CallbackInfo& info) {
		Napi::Env env = info.Env();
		Napi::Object This = info.This().As<Napi::Object>();

        if (!loaded || !m_mediaEngine->HasVideo()) return This;

        seconds = m_mediaEngine->GetCurrentTime();
        This.Set("seconds", seconds);
        
        // is a new frame ready?
        LONGLONG time;
        if (m_mediaEngine->OnVideoStreamTick(&time) == S_OK) {
            seconds = m_mediaEngine->GetCurrentTime();//time * 1e-7;
            //printf("time %f\n", seconds);
            
            This.Set("duration", duration);
            This.Set("width", width);
            This.Set("height", height);
            This.Set("paused", paused);

            //if (info.Length() > 0 && info[0].IsNumber()) 
            {
                //uint32_t texid = info[0].ToNumber().Uint32Value();

                if (!hasTexture) {
                    //printf("creating textures from %d\n", texid);

                    // create it:
                    D3D11_TEXTURE2D_DESC desc = {};
                    desc.Width = width;
                    desc.Height = height;
                    desc.MipLevels = 1;
                    desc.ArraySize = 1;
                    desc.Format = m_d3dFormat;
                    desc.SampleDesc.Count = 1;
                    desc.SampleDesc.Quality = 0;
                    desc.BindFlags = D3D11_BIND_RENDER_TARGET; //D3D11_BIND_UNORDERED_ACCESS ?
                    desc.Usage = D3D11_USAGE_DEFAULT;
                    m_spDX11Device->CreateTexture2D(&desc, NULL, &mDXTex);
                    if (FAILED(m_spDX11Device->CreateTexture2D(&desc, NULL, &mDXTex))) {
                        fprintf(stderr, "failed to create DX11 texture");
                        return This;
                    }

                    glGenTextures(1, &gl_texid);
                    mGLDX_Handle = wglDXRegisterObjectNV(
                        gl_handleD3D, 
                        mDXTex.Get(),
                        gl_texid,
                        GL_TEXTURE_2D, 
                        WGL_ACCESS_READ_ONLY_NV); //WGL_ACCESS_READ_WRITE_NV); //?

                    if (mGLDX_Handle == INVALID_HANDLE_VALUE) {
                        fprintf(stderr, "invalid handle\n");
                        return This;
                    }

                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, gl_texid);
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
                    auto gl_err = glGetError();

                    if (gl_err != GL_NO_ERROR) {
                        // <-- renderdoc seems cause glGetError to be set to GL_INVALID_OPERATION here.
                        // <-- even though the actual call to wglDXRegisterObjectNV has
                        // <-- succeeded glGetError is still set...
                        // <-- without renderdoc running this is not set.
                        //
                        // Replicated with NVIDIA GeForce RTX 3080 w/ NVIDIA 535.98 (with Renderdoc 1.27 (35b13a8e))
                        // Replicated with NVIDIA GeForce RTX 2070 w/ NVIDIA 496.76 (with Renderdoc 1.27 (35b13a8e))

                        fprintf(stderr, "glGetError triggered.\n");
                    }

                    This.Set("id", gl_texid);

                    hasTexture = true;
                }
            }

            if (hasTexture && mGLDX_Handle) {
                
                wglDXUnlockObjectsNV(gl_handleD3D, 1, &mGLDX_Handle);

                // do we need to create a texture?
                //HANDLE textureHandle; // where does this come from?
                MFVideoNormalizedRect normrect{ 0.0f, 0.0f, 1.0f, 1.0f };
                RECT targetrect{ 0, 0, width, height }; // this is actually the target texture dimension
                MFARGB bgColor{ 0, 0, 0, 0 };

                if (FAILED(m_mediaEngine->TransferVideoFrame(mDXTex.Get(), &normrect, &targetrect, &bgColor))) {
                    fprintf(stderr, "failed to transfer frame");
                    return This;
                }

                wglDXLockObjectsNV(gl_handleD3D, 1, &mGLDX_Handle);
            }

            
            // ComPtr<ID3D11Texture2D> mediaTexture;
            // if (SUCCEEDED(m_spDX11Device->OpenSharedResource1(textureHandle, IID_PPV_ARGS(mediaTexture.GetAddressOf()))))  {

            //     if (m_mediaEngine->TransferVideoFrame(mediaTexture.Get(), &rect, &rcTarget, &bgColor) == S_OK) {
            //         printf(".\n");
            //     }
                    
            // }
        }

        // if (info.Length() < 1 || !info[0].IsString()) return This;
        // const char * filename = info[0].ToString().Utf8Value().c_str();
        // printf("loading %s\n", filename);
        // BSTR bfilename = ConvertConstCharPtrToBSTR(filename);
        // loaded = false;
           
        HRESULT hr = S_OK;


        return This;
    }

    Napi::Value bind(const Napi::CallbackInfo& info) {
		Napi::Env env = info.Env();
		Napi::Object This = info.This().As<Napi::Object>();

        uint32_t unit = info[0].ToNumber().Uint32Value();
        
        if (gl_texid) {
            glActiveTexture(GL_TEXTURE0 + unit);
            glBindTexture(GL_TEXTURE_2D, gl_texid);
        }
    

        return This;
    }

    Napi::Value unbind(const Napi::CallbackInfo& info) {
		Napi::Env env = info.Env();
		Napi::Object This = info.This().As<Napi::Object>();

        uint32_t unit = info[0].ToNumber().Uint32Value();
        
        if (gl_texid) {
            glActiveTexture(GL_TEXTURE0 + unit);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
    

        return This;
    }

    Napi::Value seek(const Napi::CallbackInfo& info) {
		Napi::Env env = info.Env();
		Napi::Object This = info.This().As<Napi::Object>();
        if (info.Length() < 1 || !info[0].IsNumber()) return This;
        
        double t = info[0].ToNumber().DoubleValue();
        if (t < 0) t = 0.;
        if (t > duration) t = duration;

        if (m_engineEx) m_engineEx->SetCurrentTimeEx(t, MF_MEDIA_ENGINE_SEEK_MODE_NORMAL);

        return This;
    }

    Napi::Value pause(const Napi::CallbackInfo& info) {
		Napi::Env env = info.Env();
		Napi::Object This = info.This().As<Napi::Object>();
        
        paused = info[0].ToBoolean();

        if (m_engineEx) {
            if (paused) {
                m_engineEx->Pause();
            } else {
                m_engineEx->Play();
            }
        }

        This.Set("paused", paused);

        return This;
    }

    virtual void OnMediaEngineEvent(DWORD meEvent, DWORD_PTR param1, DWORD param2) {
        if (meEvent == MF_MEDIA_ENGINE_EVENT_TIMEUPDATE) return; // keep it quiet

        printf("OnMediaEngineEvent %s\n", MFEventToString((MF_MEDIA_ENGINE_EVENT)meEvent).c_str());
        switch (meEvent) {
            case MF_MEDIA_ENGINE_EVENT_LOADEDMETADATA: {
                duration = (m_mediaEngine->GetDuration());
                printf("duration %f\n", duration);
                DWORD w, h;
                if (SUCCEEDED(m_mediaEngine->GetNativeVideoSize(&w, &h))) {
                    //printf("dimension %d x %d\n", w, h);
                    width = w;
                    height = h;
                }
                loaded = true;
                
                //m_engineEx->SetCurrentTimeEx(seconds, MF_MEDIA_ENGINE_SEEK_MODE_NORMAL);
                break;
            }
            case MF_MEDIA_ENGINE_EVENT_ERROR:
            {
                ComPtr<IMFMediaError> error;
                m_mediaEngine->GetError(&error);
                USHORT errorCode = error->GetErrorCode();
                fprintf(stderr, "ERROR: Media Foundation Event Error %u %s\n", errorCode, MFErrorToString((MF_MEDIA_ENGINE_ERR)param1).c_str());
                break;
            }
            //default:
                
        }
    }

};

class Module : public Napi::Addon<Module> {
public:
    Module(Napi::Env env, Napi::Object exports) {

        
        
		// See https://github.com/nodejs/node-addon-api/blob/main/doc/class_property_descriptor.md
		DefineAddon(exports, {
		// 	// InstanceMethod("start", &Module::start),
		// 	// InstanceMethod("end", &Module::end),
		// 	// //InstanceMethod("test", &Module::test),
		// 	// // InstanceValue
		// 	// // InstanceAccessor
		// 	InstanceAccessor<&Module::devices>("devices"),
		// 	// InstanceAccessor<&Module::Gett>("t"),
		// 	// InstanceAccessor<&Module::GetSamplerate>("samplerate"),
		});

        {
            // This method is used to hook the accessor and method callbacks
            Napi::Function ctor = Video::DefineClass(env, "Video", {
                Video::InstanceMethod<&Video::load>("load"),
                Video::InstanceMethod<&Video::update>("update"),
                Video::InstanceMethod<&Video::seek>("seek"),
                Video::InstanceMethod<&Video::pause>("pause"),

                Video::InstanceMethod<&Video::bind>("bind"),
                Video::InstanceMethod<&Video::unbind>("unbind"),
            });

            // Create a persistent reference to the class constructor.
            Napi::FunctionReference* constructor = new Napi::FunctionReference();
            *constructor = Napi::Persistent(ctor);
            exports.Set("Video", ctor);
            env.SetInstanceData<Napi::FunctionReference>(constructor);
        }

	}
};

NODE_API_ADDON(Module)