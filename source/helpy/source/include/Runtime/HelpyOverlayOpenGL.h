#pragma once
#include "Runtime/HelpyOverlayGpu.h"
#include <gl/GL.h>
#include <GL/glext.h>

namespace PS::HelpyOverlayGpu {
class ConsumerGL {
    #define HELPY_GL_FUNCTIONS(X) \
        X(PFNGLCREATEMEMORYOBJECTSEXTPROC,CreateMemoryObjectsEXT) X(PFNGLDELETEMEMORYOBJECTSEXTPROC,DeleteMemoryObjectsEXT) \
        X(PFNGLMEMORYOBJECTPARAMETERIVEXTPROC,MemoryObjectParameterivEXT) X(PFNGLIMPORTMEMORYWIN32NAMEEXTPROC,ImportMemoryWin32NameEXT) \
        X(PFNGLTEXSTORAGEMEM2DEXTPROC,TexStorageMem2DEXT) X(PFNGLGENSEMAPHORESEXTPROC,GenSemaphoresEXT) X(PFNGLDELETESEMAPHORESEXTPROC,DeleteSemaphoresEXT) \
        X(PFNGLIMPORTSEMAPHOREWIN32NAMEEXTPROC,ImportSemaphoreWin32NameEXT) X(PFNGLSEMAPHOREPARAMETERUI64VEXTPROC,SemaphoreParameterui64vEXT) \
        X(PFNGLWAITSEMAPHOREEXTPROC,WaitSemaphoreEXT) X(PFNGLSIGNALSEMAPHOREEXTPROC,SignalSemaphoreEXT) X(PFNGLGETUNSIGNEDBYTEVEXTPROC,GetUnsignedBytevEXT) \
        X(PFNGLCREATESHADERPROC,CreateShader) X(PFNGLSHADERSOURCEPROC,ShaderSource) X(PFNGLCOMPILESHADERPROC,CompileShader) X(PFNGLGETSHADERIVPROC,GetShaderiv) X(PFNGLDELETESHADERPROC,DeleteShader) \
        X(PFNGLCREATEPROGRAMPROC,CreateProgram) X(PFNGLATTACHSHADERPROC,AttachShader) X(PFNGLLINKPROGRAMPROC,LinkProgram) X(PFNGLGETPROGRAMIVPROC,GetProgramiv) X(PFNGLDELETEPROGRAMPROC,DeleteProgram) X(PFNGLUSEPROGRAMPROC,UseProgram) \
        X(PFNGLGETUNIFORMLOCATIONPROC,GetUniformLocation) X(PFNGLUNIFORM1IPROC,Uniform1i) X(PFNGLUNIFORM1FPROC,Uniform1f) X(PFNGLGENVERTEXARRAYSPROC,GenVertexArrays) X(PFNGLDELETEVERTEXARRAYSPROC,DeleteVertexArrays) X(PFNGLBINDVERTEXARRAYPROC,BindVertexArray) \
        X(PFNGLACTIVETEXTUREPROC,ActiveTexture) X(PFNGLBINDSAMPLERPROC,BindSampler) X(PFNGLBINDFRAMEBUFFERPROC,BindFramebuffer) X(PFNGLDRAWBUFFERSPROC,DrawBuffers) X(PFNGLBLENDFUNCSEPARATEPROC,BlendFuncSeparate) X(PFNGLBLENDEQUATIONSEPARATEPROC,BlendEquationSeparate) X(PFNGLGETSTRINGIPROC,GetStringi)
    #define DECLARE(type,name) type name=nullptr;
    HELPY_GL_FUNCTIONS(DECLARE)
    #undef DECLARE
    GLuint Memories[3]{},Textures[3]{},Semaphores[2]{},Program=0,Vao=0;GLint Opacity=-1;
    HANDLE Mapping=nullptr,App=nullptr;HelpyOverlayFrames::Page* Control=nullptr;uint64_t Sequence=0;
    HGLRC Context=nullptr;
    struct Saved {
        GLint Program{},Vao{},Texture{},Sampler{},Active{},Framebuffer{},DrawBuffer{},Viewport[4]{},SrcRgb{},DstRgb{},SrcAlpha{},DstAlpha{},EqRgb{},EqAlpha{};
        GLboolean Mask[4]{};std::array<GLboolean,10> Enabled{};GLint Polygon[2]{},BufferCount=1;std::array<GLenum,32> Buffers{};
    };
    inline static constexpr std::array<GLenum,10> Capabilities{GL_BLEND,GL_CULL_FACE,GL_DEPTH_TEST,GL_STENCIL_TEST,GL_SCISSOR_TEST,GL_RASTERIZER_DISCARD,GL_FRAMEBUFFER_SRGB,GL_SAMPLE_ALPHA_TO_COVERAGE,GL_COLOR_LOGIC_OP,GL_SAMPLE_COVERAGE};
    Saved Save(){Saved s;glGetIntegerv(GL_POLYGON_MODE,s.Polygon);glGetIntegerv(GL_CURRENT_PROGRAM,&s.Program);glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&s.Vao);glGetIntegerv(GL_ACTIVE_TEXTURE,&s.Active);ActiveTexture(GL_TEXTURE0);glGetIntegerv(GL_TEXTURE_BINDING_2D,&s.Texture);glGetIntegerv(GL_SAMPLER_BINDING,&s.Sampler);glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&s.Framebuffer);glGetIntegerv(GL_DRAW_BUFFER,&s.DrawBuffer);if(s.Framebuffer){glGetIntegerv(GL_MAX_DRAW_BUFFERS,&s.BufferCount);s.BufferCount=std::clamp(s.BufferCount,1,32);for(GLint i=0;i<s.BufferCount;++i){GLint value{};glGetIntegerv(GL_DRAW_BUFFER0+i,&value);s.Buffers[i]=value;}}glGetIntegerv(GL_VIEWPORT,s.Viewport);glGetBooleanv(GL_COLOR_WRITEMASK,s.Mask);glGetIntegerv(GL_BLEND_SRC_RGB,&s.SrcRgb);glGetIntegerv(GL_BLEND_DST_RGB,&s.DstRgb);glGetIntegerv(GL_BLEND_SRC_ALPHA,&s.SrcAlpha);glGetIntegerv(GL_BLEND_DST_ALPHA,&s.DstAlpha);glGetIntegerv(GL_BLEND_EQUATION_RGB,&s.EqRgb);glGetIntegerv(GL_BLEND_EQUATION_ALPHA,&s.EqAlpha);for(size_t i=0;i<Capabilities.size();++i)s.Enabled[i]=glIsEnabled(Capabilities[i]);return s;}
    void Restore(const Saved& s){UseProgram(s.Program);BindVertexArray(s.Vao);glBindTexture(GL_TEXTURE_2D,s.Texture);BindSampler(0,s.Sampler);ActiveTexture(s.Active);BindFramebuffer(GL_DRAW_FRAMEBUFFER,s.Framebuffer);if(s.Framebuffer)DrawBuffers(s.BufferCount,s.Buffers.data());else glDrawBuffer(s.DrawBuffer);if(s.Polygon[0]==s.Polygon[1])glPolygonMode(GL_FRONT_AND_BACK,s.Polygon[0]);else {glPolygonMode(GL_FRONT,s.Polygon[0]);glPolygonMode(GL_BACK,s.Polygon[1]);}glViewport(s.Viewport[0],s.Viewport[1],s.Viewport[2],s.Viewport[3]);glColorMask(s.Mask[0],s.Mask[1],s.Mask[2],s.Mask[3]);BlendFuncSeparate(s.SrcRgb,s.DstRgb,s.SrcAlpha,s.DstAlpha);BlendEquationSeparate(s.EqRgb,s.EqAlpha);for(size_t i=0;i<Capabilities.size();++i)if(s.Enabled[i])glEnable(Capabilities[i]);else glDisable(Capabilities[i]);}
    GLuint Shader(GLenum kind,const char* text){const auto shader=CreateShader(kind);ShaderSource(shader,1,&text,nullptr);CompileShader(shader);GLint compiled{};GetShaderiv(shader,GL_COMPILE_STATUS,&compiled);if(!compiled){DeleteShader(shader);throw std::runtime_error("Cannot compile native OpenGL overlay shader");}return shader;}
public:
    ~ConsumerGL(){Reset();}
    bool Active()const{return Control!=nullptr;}
    HelpyOverlayFrames::Page* Page()const{return Control;}
    void Load(){
        if(!wglGetCurrentContext())throw std::runtime_error("OpenGL presentation has no current context");
        #define LOAD(type,name) name=reinterpret_cast<type>(wglGetProcAddress("gl" #name));if(reinterpret_cast<uintptr_t>(name)<=3||reinterpret_cast<uintptr_t>(name)==uintptr_t(-1))throw std::runtime_error("OpenGL driver does not expose gl" #name);
        HELPY_GL_FUNCTIONS(LOAD)
        #undef LOAD
        GLint count{};glGetIntegerv(GL_NUM_EXTENSIONS,&count);bool memory=false,semaphore=false;for(int i=0;i<count;++i){const auto* extension=reinterpret_cast<const char*>(GetStringi(GL_EXTENSIONS,i));if(extension){memory|=std::strcmp(extension,"GL_EXT_memory_object_win32")==0;semaphore|=std::strcmp(extension,"GL_EXT_semaphore_win32")==0;}}
        if(!memory||!semaphore)throw std::runtime_error("OpenGL driver requires EXT_memory_object_win32 and EXT_semaphore_win32 for native shared-GPU presentation");
    }
    LUID Adapter(){LUID adapter{};GetUnsignedBytevEXT(GL_DEVICE_LUID_EXT,reinterpret_cast<GLubyte*>(&adapter));return adapter;}
    void Hide(){if(Control)InterlockedExchange(&Control->Visible,0);}
    void Reset(){
        Hide();if(Control)InterlockedExchange(&Control->Bound,0);
        if(Context&&Context==wglGetCurrentContext()){if(Program)DeleteProgram(Program);if(Vao)DeleteVertexArrays(1,&Vao);glDeleteTextures(3,Textures);DeleteMemoryObjectsEXT(3,Memories);DeleteSemaphoresEXT(2,Semaphores);}
        if(Control){UnmapViewOfFile(Control);Control=nullptr;}if(Mapping){CloseHandle(Mapping);Mapping=nullptr;}if(App){CloseHandle(App);App=nullptr;}Program=Vao=0;std::fill_n(Textures,3,0);std::fill_n(Memories,3,0);std::fill_n(Semaphores,2,0);Sequence=0;Context=nullptr;
    }
    void Create(uint32_t app,const std::string& nonce){
        Load();Reset();Context=wglGetCurrentContext();const auto key=Name(GetCurrentProcessId(),app,nonce);Mapping=OpenFileMappingW(FILE_MAP_ALL_ACCESS,FALSE,(key+L".control").c_str());if(!Mapping)throw std::runtime_error("OpenGL overlay control page unavailable");Control=static_cast<HelpyOverlayFrames::Page*>(MapViewOfFile(Mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(HelpyOverlayFrames::Page)));if(!Control||!HelpyOverlayFrames::Shape(*Control,GetCurrentProcessId(),app))throw std::runtime_error("OpenGL overlay control page invalid");App=OpenProcess(SYNCHRONIZE,FALSE,app);if(!App)throw std::runtime_error("Overlay app is unavailable");
        const auto saved=Save();const auto restore=[&](void*){Restore(saved);};std::unique_ptr<void,decltype(restore)> state(reinterpret_cast<void*>(1),restore);
        const GLint dedicated=GL_TRUE;CreateMemoryObjectsEXT(3,Memories);glGenTextures(3,Textures);
        for(unsigned i=0;i<3;++i){MemoryObjectParameterivEXT(Memories[i],GL_DEDICATED_MEMORY_OBJECT_EXT,&dedicated);ImportMemoryWin32NameEXT(Memories[i],0,GL_HANDLE_TYPE_D3D11_IMAGE_EXT,(key+L".texture"+std::to_wstring(i)).c_str());glBindTexture(GL_TEXTURE_2D,Textures[i]);TexStorageMem2DEXT(GL_TEXTURE_2D,1,GL_RGBA8,Control->Width,Control->Height,Memories[i],0);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);}
        GenSemaphoresEXT(2,Semaphores);ImportSemaphoreWin32NameEXT(Semaphores[0],GL_HANDLE_TYPE_D3D12_FENCE_EXT,(key+L".ready").c_str());ImportSemaphoreWin32NameEXT(Semaphores[1],GL_HANDLE_TYPE_D3D12_FENCE_EXT,(key+L".done").c_str());
        const auto vertex=Shader(GL_VERTEX_SHADER,"#version 330 core\nout vec2 uv;void main(){uv=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(uv*vec2(2,-2)+vec2(-1,1),0,1);}");
        const auto pixel=Shader(GL_FRAGMENT_SHADER,"#version 330 core\nin vec2 uv;uniform sampler2D picture;uniform float opacity;out vec4 color;void main(){color=texture(picture,uv)*opacity;}");
        Program=CreateProgram();AttachShader(Program,vertex);AttachShader(Program,pixel);LinkProgram(Program);DeleteShader(vertex);DeleteShader(pixel);GLint linked{};GetProgramiv(Program,GL_LINK_STATUS,&linked);if(!linked)throw std::runtime_error("Cannot link OpenGL overlay program");UseProgram(Program);Uniform1i(GetUniformLocation(Program,"picture"),0);Opacity=GetUniformLocation(Program,"opacity");GenVertexArrays(1,&Vao);InterlockedExchange(&Control->Bound,1);
    }
    bool Draw(uint32_t width,uint32_t height){
        using namespace HelpyOverlayFrames;if(!Control||Context!=wglGetCurrentContext()||WaitForSingleObject(App,0)!=WAIT_TIMEOUT||!Alive(*Control,GetTickCount64())){Hide();return false;}
        const auto slot=Latest(*Control,UINT64_MAX);if(slot<0||slot>=3)return false;const auto left=std::min(Control->Left,width),top=std::min(Control->Top,height),w=std::min(Control->Width,width-left),h=std::min(Control->Height,height-top);if(!w||!h){Hide();return false;}
        InterlockedExchange(&Control->ViewportWidth,width);InterlockedExchange(&Control->ViewportHeight,height);const auto saved=Save();const auto restore=[&](void*){Restore(saved);};std::unique_ptr<void,decltype(restore)> state(reinterpret_cast<void*>(1),restore);
        const GLuint64 ready=Read(Control->Ready[slot]);const GLenum layout=GL_LAYOUT_GENERAL_EXT;SemaphoreParameterui64vEXT(Semaphores[0],GL_D3D12_FENCE_VALUE_EXT,&ready);WaitSemaphoreEXT(Semaphores[0],0,nullptr,1,&Textures[slot],&layout);
        glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);for(auto capability:Capabilities)glDisable(capability);glEnable(GL_BLEND);BlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);BlendFuncSeparate(GL_ONE,GL_ONE_MINUS_SRC_ALPHA,GL_ONE,GL_ONE_MINUS_SRC_ALPHA);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);BindFramebuffer(GL_DRAW_FRAMEBUFFER,0);glDrawBuffer(GL_BACK);glViewport(left,height-top-h,w,h);UseProgram(Program);BindVertexArray(Vao);glBindTexture(GL_TEXTURE_2D,Textures[slot]);BindSampler(0,0);Uniform1f(Opacity,std::clamp(Read(Control->Opacity)/10000.f,.6f,1.f));glDrawArrays(GL_TRIANGLES,0,3);
        const GLuint64 sequence=++Sequence;SemaphoreParameterui64vEXT(Semaphores[1],GL_D3D12_FENCE_VALUE_EXT,&sequence);SignalSemaphoreEXT(Semaphores[1],0,nullptr,1,&Textures[slot],&layout);glFlush();Presented(*Control,slot,sequence);return true;
    }
};
#undef HELPY_GL_FUNCTIONS
}
