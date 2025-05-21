{
  "targets": [
    {
        "target_name": "video",
        "sources": [],
        "defines": [],
        "cflags": ["-std=c++11", "-Wall", "-pedantic", "-O3"],
        "include_dirs": [ 
          "<!(node -p \"require('node-addon-api').include_dir\")",
        ],
        "libraries": [],
        "dependencies": [],
        "conditions": [
            ['OS=="win"', {
              "sources": [ "node-video.cpp" ],
              'include_dirs': [
                './node_modules/native-graphics-deps/include',
              ],
              'library_dirs': [
                './lib',
                './node_modules/native-graphics-deps/lib/windows/glew',
                'lib',
              ],
              'libraries': [
                  'comsuppw.lib',
                'glew32.lib',
                'opengl32.lib',
                'd3d11.lib',
                'mf.lib',
                'mfreadwrite.lib',
                'mfplat.lib',
                'mfuuid.lib'
              ],
              'defines' : [
                'WIN32_LEAN_AND_MEAN',
                'VC_EXTRALEAN'
              ],
              "copies": [{
                'destination': './build/<(CONFIGURATION_NAME)/',
                'files': ['./node_modules/native-graphics-deps/lib/windows/glew/glew32.dll']
              }]
            }],
            ['OS=="mac"', {
              'cflags+': ['-fvisibility=hidden'],
              'xcode_settings': {},
            }],
            ['OS=="linux"', {}],
        ],
    }
  ]
}