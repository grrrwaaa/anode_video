const fs = require("fs")
const path = require("path")
const assert = require("assert")
const video = require("./index.js")

const { gl, glfw, Window, glutils, Shaderman, Prng, Config } = require("../anode_gl")

// have to create a gl context first:
let window = new Window({
    sync: true,
})
assert(glfw.extensionSupported("WGL_NV_DX_interop"), "dx11 opengl interp not supported")

let shaderman = new Shaderman(gl)
let quad_vao = glutils.createVao(gl, glutils.makeQuad())


const vid = new video.Video()

let filename = path.resolve(process.argv[2])
vid.load(filename)


window.draw = function() {
    let { t, dt, dim, title } = this
    let [width, height] = dim
    let aspect = width/height

    if (this.pointer.buttons[1] && vid.duration) {
        let x = Math.max(0, Math.min(1, this.pointer.pos[0]))
        vid.seek(x * vid.duration)
    }

    vid.update()

    gl.viewport(0, 0, dim[0], dim[1]);
    gl.enable(gl.DEPTH_TEST)
    gl.clearColor(0.5, 0, 0, 1);
    gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);

    vid.bind()
    shaderman.shaders.show.begin()
    quad_vao.bind().draw()

    if (Math.floor(t) > Math.floor(t - dt)) {
        console.log(1/dt)
    }
}

Window.animate()