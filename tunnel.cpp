// C++ hoitaa ikkunan, ajan ja kameran. tunnel.frag laskee pikselit GPU:lla.
#define GL_GLEXT_PROTOTYPES
#include <SDL.h>
#include <SDL_opengl.h>

#include "shader_sources.hpp"
#include "tunnel_math.hpp"

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace tunnel;

namespace {

void require(bool ok, const std::string& what) {
    if (!ok) throw std::runtime_error(what+": "+SDL_GetError());
}

void help() {
    std::cout <<
        "PsyTunnel — C++ / SDL2 / OpenGL\n"
        "Käyttö: psytunnel [valitsimet]\n\n"
        "  --fullscreen           Aloita koko näytössä\n"
        "  --resolution 1920x1080  Lukitse piirtoresoluutio (oletus: ikkunan/näytön pikselit)\n"
        "  --fps 156              FPS-raja; 0 = näytön VSync (oletus)\n"
        "  --speed 1.5            Etenemisnopeus 0–8 (oletus 1.5)\n"
        "  --sway 0.85            Kameran huojunta 0–1 (oletus 0.85)\n"
        "  --palette auto         auto, grayscale, rainbow, blue, green, yellow, red\n"
        "  --time 28              Aloita tästä animaatioajasta (sekunteina)\n"
        "  --frames 120           Sulje annetun ruutumäärän jälkeen\n"
        "  --screenshot kuva.bmp  Tallenna viimeinen ruutu (oletus: yksi ruutu)\n"
        "  --headless             Piirrä ilman näyttöikkunaa, oletuksena yksi ruutu\n"
        "  --help                 Tämä ohje\n\n"
        "F11: koko näyttö | Esc: lopeta | Space/Pause: pysäytä/jatka\n"
        "1–6: valitse väri | 0: automaattinen värikierto\n"
        "Ylös/alas: nopeus | Vasen/oikea: huojunta | R: etenemissuunta\n"
        "F1: tulosta ohje terminaaliin\n";
}

struct Options {
    Resolution resolution;
    int fps=0, frames=0, palette=-1;
    double speed=1.5, sway=0.85, time=0.0;
    bool fullscreen=false, headless=false;
    std::string screenshot;
};

double number(const std::string& text, double low, double high, const std::string& option) {
    std::size_t end=0;
    double value;
    try { value=std::stod(text, &end); }
    catch (const std::exception&) { throw std::runtime_error(option+": virheellinen luku"); }
    if (end!=text.size() || !std::isfinite(value) || value<low || value>high)
        throw std::runtime_error(option+": luku on sallitun alueen ulkopuolella");
    return value;
}

Options options(int argc, char** argv) {
    Options o;
    for (int i=1; i<argc; ++i) {
        const std::string arg=argv[i];
        if (arg=="--fullscreen") { o.fullscreen=true; continue; }
        if (arg=="--headless") { o.headless=true; continue; }
        if (arg!="--resolution" && arg!="--fps" && arg!="--frames" && arg!="--speed" &&
            arg!="--sway" && arg!="--palette" && arg!="--time" && arg!="--screenshot")
            throw std::runtime_error("Tuntematon valitsin: "+arg+" (katso --help)");
        if (++i>=argc) throw std::runtime_error("Valitsimelta puuttuu arvo: "+arg);
        const std::string value=argv[i];
        if (arg=="--resolution") o.resolution=parseResolution(value);
        else if (arg=="--speed") o.speed=number(value,0,8,arg);
        else if (arg=="--sway") o.sway=number(value,0,1,arg);
        else if (arg=="--time") o.time=number(value,0,86400,arg);
        else if (arg=="--screenshot") o.screenshot=value;
        else if (arg=="--fps" || arg=="--frames") {
            const double n=number(value,arg=="--fps"?0:1,arg=="--fps"?500:10000000,arg);
            if (n!=std::floor(n)) throw std::runtime_error(arg+": anna kokonaisluku");
            if (arg=="--fps") o.fps=static_cast<int>(n);
            else o.frames=static_cast<int>(n);
        } else if (arg=="--palette") {
            if (value=="auto") o.palette=-1;
            else {
                const auto found=std::find(PaletteNames.begin(),PaletteNames.end(),value);
                if (found==PaletteNames.end()) throw std::runtime_error("Tuntematon paletti: "+value);
                o.palette=static_cast<int>(found-PaletteNames.begin());
            }
        }
    }
    if (o.frames==0 && (o.headless || !o.screenshot.empty())) o.frames=1;
    return o;
}

// Destruktori (~Sdl) on Pythonin with/finally-rakenteen kaltainen:
// resurssit vapautuvat myös poikkeuksen sattuessa ja aikaisessa returnissa.
struct Sdl {
    Sdl() { require(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)==0,"SDL:n alustus"); }
    ~Sdl() { SDL_Quit(); }
    Sdl(const Sdl&)=delete;
    Sdl& operator=(const Sdl&)=delete;
};

struct Context {
    SDL_GLContext handle;
    explicit Context(SDL_Window* w): handle(SDL_GL_CreateContext(w)) {
        require(handle!=nullptr,"OpenGL 3.3 -kontekstin luonti");
    }
    ~Context() { SDL_GL_DeleteContext(handle); }
    Context(const Context&)=delete;
    Context& operator=(const Context&)=delete;
};

GLuint compile(GLenum type, const char* source) {
    const GLuint shader=glCreateShader(type);
    glShaderSource(shader,1,&source,nullptr);
    glCompileShader(shader);
    GLint ok=0;
    glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
    if (!ok) {
        GLint size=0;
        glGetShaderiv(shader,GL_INFO_LOG_LENGTH,&size);
        std::string log(static_cast<std::size_t>(std::max(1,size)), '\0');
        glGetShaderInfoLog(shader,size,nullptr,log.data());
        glDeleteShader(shader);
        throw std::runtime_error("Shaderin käännös: "+log);
    }
    return shader;
}

class Renderer {
    GLuint program_=0, vao_=0, fbo_=0, texture_=0;
    int width_=0, height_=0;
    GLint resolution_, time_, camera_, forward_, right_, up_, from_, to_, mix_;
    void clean() {
        glDeleteTextures(1,&texture_);
        glDeleteFramebuffers(1,&fbo_);
        glDeleteVertexArrays(1,&vao_);
        if (program_) glDeleteProgram(program_);
    }
    static void vector(GLint where, Vec3 value) {
        glUniform3f(where,static_cast<float>(value.x),static_cast<float>(value.y),
                    static_cast<float>(value.z));
    }
public:
    Renderer() {
        GLuint vs=0, fs=0;
        try {
            vs=compile(GL_VERTEX_SHADER,VertexSource);
            fs=compile(GL_FRAGMENT_SHADER,FragmentSource);
            program_=glCreateProgram();
            glAttachShader(program_,vs); glAttachShader(program_,fs);
            glLinkProgram(program_);
            GLint linked=0;
            glGetProgramiv(program_,GL_LINK_STATUS,&linked);
            if (!linked) {
                GLint size=0;
                glGetProgramiv(program_,GL_INFO_LOG_LENGTH,&size);
                std::string log(static_cast<std::size_t>(std::max(1,size)),'\0');
                glGetProgramInfoLog(program_,size,nullptr,log.data());
                throw std::runtime_error("Shaderien linkitys: "+log);
            }
            glGenVertexArrays(1,&vao_);
            glGenFramebuffers(1,&fbo_);
            glGenTextures(1,&texture_);
        } catch (...) {
            if (vs) glDeleteShader(vs);
            if (fs) glDeleteShader(fs);
            clean(); throw;
        }
        glDeleteShader(vs); glDeleteShader(fs);
        resolution_=glGetUniformLocation(program_,"uResolution");
        time_=glGetUniformLocation(program_,"uTime");
        camera_=glGetUniformLocation(program_,"uCamera");
        forward_=glGetUniformLocation(program_,"uForward");
        right_=glGetUniformLocation(program_,"uRight");
        up_=glGetUniformLocation(program_,"uUp");
        from_=glGetUniformLocation(program_,"uPaletteFrom");
        to_=glGetUniformLocation(program_,"uPaletteTo");
        mix_=glGetUniformLocation(program_,"uPaletteMix");
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
    }
    ~Renderer() { clean(); }
    Renderer(const Renderer&)=delete;
    Renderer& operator=(const Renderer&)=delete;

    void resize(int w, int h) {
        if (w==width_ && h==height_) return;
        GLint maxSize=0;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE,&maxSize);
        if (w>maxSize || h>maxSize) throw std::runtime_error("Piirtoresoluutio ylittää GPU:n tekstuurirajan");
        glBindTexture(GL_TEXTURE_2D,texture_);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glBindFramebuffer(GL_FRAMEBUFFER,fbo_);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture_,0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)
            throw std::runtime_error("Piirtopuskurin luonti epäonnistui");
        if (glGetError()!=GL_NO_ERROR) throw std::runtime_error("OpenGL: piirtoresoluution vaihto epäonnistui");
        width_=w; height_=h;
    }

    void draw(double seconds, const Camera& camera, PaletteBlend palette, int screenW, int screenH) {
        glBindFramebuffer(GL_FRAMEBUFFER,fbo_);
        glViewport(0,0,width_,height_);
        glUseProgram(program_);
        glBindVertexArray(vao_);
        glUniform2f(resolution_,static_cast<float>(width_),static_cast<float>(height_));
        glUniform1f(time_,static_cast<float>(seconds));
        vector(camera_,camera.position); vector(forward_,camera.forward);
        vector(right_,camera.right); vector(up_,camera.up);
        glUniform1i(from_,palette.from); glUniform1i(to_,palette.to);
        glUniform1f(mix_,static_cast<float>(palette.mix));
        glDrawArrays(GL_TRIANGLES,0,3);

        // Lukittu --resolution säilyttää kuvasuhteensa myös erikokoisella
        // näytöllä. Ilman valitsinta lähde ja kohde ovat täsmälleen samankokoiset.
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER,0);
        glClearColor(0,0,0,1); glClear(GL_COLOR_BUFFER_BIT);
        const double scale=std::min(double(screenW)/width_,double(screenH)/height_);
        const int w=std::max(1,int(std::round(width_*scale)));
        const int h=std::max(1,int(std::round(height_*scale)));
        const int x=(screenW-w)/2, y=(screenH-h)/2;
        glBlitFramebuffer(0,0,width_,height_,x,y,x+w,y+h,GL_COLOR_BUFFER_BIT,GL_LINEAR);
        glBindFramebuffer(GL_FRAMEBUFFER,0);
    }

    void screenshot(const std::string& path) {
        std::vector<unsigned char> pixels(static_cast<std::size_t>(width_)*height_*4);
        glBindFramebuffer(GL_READ_FRAMEBUFFER,fbo_);
        glReadPixels(0,0,width_,height_,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
        if (glGetError()!=GL_NO_ERROR) throw std::runtime_error("Kuvakaappauksen lukeminen epäonnistui");
        // OpenGL aloittaa vasemmasta alakulmasta, BMP:n SDL-pinta yläkulmasta.
        const std::size_t row=static_cast<std::size_t>(width_)*4;
        for (int y=0; y<height_/2; ++y)
            std::swap_ranges(pixels.begin()+y*row,pixels.begin()+(y+1)*row,
                             pixels.begin()+(height_-1-y)*row);
        using Surface=std::unique_ptr<SDL_Surface,decltype(&SDL_FreeSurface)>;
        Surface surface(SDL_CreateRGBSurfaceWithFormatFrom(pixels.data(),width_,height_,32,
                        width_*4,SDL_PIXELFORMAT_RGBA32),SDL_FreeSurface);
        require(bool(surface),"Kuvakaappauksen pinta");
        require(SDL_SaveBMP(surface.get(),path.c_str())==0,"Kuvakaappauksen tallennus");
        glBindFramebuffer(GL_READ_FRAMEBUFFER,0);
        std::cout << "Kuvakaappaus: " << path << " (" << width_ << 'x' << height_ << ")\n";
    }
};

bool fullscreen(SDL_Window* window, bool enabled) {
    if (SDL_SetWindowFullscreen(window,enabled?SDL_WINDOW_FULLSCREEN_DESKTOP:0)!=0) {
        std::cerr << "Koko näyttö: " << SDL_GetError() << '\n';
        return false;
    }
    SDL_ShowCursor(enabled?SDL_DISABLE:SDL_ENABLE);
    return true;
}

} // namespace

int main(int argc, char** argv) {
    try {
        for (int i=1; i<argc; ++i) if (std::string(argv[i])=="--help") { help(); return 0; }
        const Options config=options(argc,argv);
        if (config.headless) SDL_setenv("SDL_VIDEODRIVER","offscreen",1);
        Sdl sdl;
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
        const int windowW=config.resolution.width?config.resolution.width:1280;
        const int windowH=config.resolution.height?config.resolution.height:720;
        using Window=std::unique_ptr<SDL_Window,decltype(&SDL_DestroyWindow)>;
        Window window(SDL_CreateWindow("PsyTunnel",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
                      windowW,windowH,SDL_WINDOW_OPENGL|SDL_WINDOW_ALLOW_HIGHDPI|
                      SDL_WINDOW_RESIZABLE|(config.headless?SDL_WINDOW_HIDDEN:0)),SDL_DestroyWindow);
        require(bool(window),"Ikkunan luonti");
        SDL_SetWindowMinimumSize(window.get(),320,240);
        Context context(window.get());
        int fpsLimit=config.headless?0:config.fps;
        if (SDL_GL_SetSwapInterval(config.headless || fpsLimit?0:1)!=0 && !config.headless && !fpsLimit) {
            fpsLimit=156;
            std::cerr << "VSync ei käytettävissä; käytetään 156 FPS:n rajaa.\n";
        }
        Renderer renderer;
        std::cout << "GPU: " << glGetString(GL_RENDERER) << '\n';
        std::cout << "Esc: lopeta | F11: koko näyttö | F1: ohje | 0–6: värit\n";
        bool isFullscreen=false;
        if (config.fullscreen && !config.headless && fullscreen(window.get(),true)) isFullscreen=true;
        bool running=true, paused=false;
        int frames=0, palette=config.palette, direction=1;
        double seconds=config.time, distance=config.time*config.speed;
        double speed=config.speed, sway=config.sway, desiredSway=sway, velocity=speed;
        using Clock=std::chrono::steady_clock;
        auto previous=Clock::now(), began=previous, titleUpdated=previous;
        int titleFrames=0;
        while (running && (!config.frames || frames<config.frames)) {
            const auto frameStart=Clock::now();
            const double dt=frames==0?0.0:std::min(0.1,std::chrono::duration<double>(frameStart-previous).count());
            previous=frameStart;
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type==SDL_QUIT) running=false;
                if (event.type!=SDL_KEYDOWN) continue;
                const auto key=event.key.keysym.sym;
                if (key==SDLK_ESCAPE) running=false;
                else if (key==SDLK_UP) speed=std::min(8.0,speed+0.15);
                else if (key==SDLK_DOWN) speed=std::max(0.0,speed-0.15);
                else if (key==SDLK_RIGHT) desiredSway=std::min(1.0,desiredSway+0.05);
                else if (key==SDLK_LEFT) desiredSway=std::max(0.0,desiredSway-0.05);
                else if (!event.key.repeat) {
                    if (key==SDLK_F11 && !config.headless && fullscreen(window.get(),!isFullscreen))
                        isFullscreen=!isFullscreen;
                    else if (key==SDLK_SPACE || key==SDLK_PAUSE) paused=!paused;
                    else if (key==SDLK_r) direction=-direction;
                    else if (key==SDLK_F1) help();
                    else if (key==SDLK_0) palette=-1;
                    else if (key>=SDLK_1 && key<=SDLK_6) palette=static_cast<int>(key-SDLK_1);
                }
            }
            if (!running) break;
            if (!paused) {
                seconds+=dt;
                const double approach=1.0-std::exp(-dt*3.0);
                const double nextVelocity=velocity+(direction*speed-velocity)*approach;
                distance+=(velocity+nextVelocity)*0.5*dt;
                velocity=nextVelocity;
                sway+=(desiredSway-sway)*approach;
            }
            int w=0,h=0;
            SDL_GL_GetDrawableSize(window.get(),&w,&h);
            if (w<=0 || h<=0 || (SDL_GetWindowFlags(window.get())&SDL_WINDOW_MINIMIZED)) {
                SDL_Delay(20); continue;
            }
            renderer.resize(config.resolution.width?config.resolution.width:w,
                            config.resolution.height?config.resolution.height:h);
            const auto colors=palette<0?paletteAt(seconds):PaletteBlend{palette,palette,0};
            renderer.draw(seconds,cameraAt(seconds,distance,sway),colors,w,h);
            SDL_GL_SwapWindow(window.get());
            ++frames; ++titleFrames;
            const double titleSeconds=std::chrono::duration<double>(frameStart-titleUpdated).count();
            if (titleSeconds>=1.0) {
                const std::string title=std::string("PsyTunnel | ")+(paused?"PAUSE | ":"")+
                    (palette<0?std::string("auto: "):std::string())+PaletteNames[colors.from]+
                    " | "+std::to_string(int(titleFrames/titleSeconds))+" FPS | nopeus "+
                    std::to_string(int(std::round(speed*100)))+"% | huojunta "+
                    std::to_string(int(std::round(desiredSway*100)))+"%";
                SDL_SetWindowTitle(window.get(),title.c_str());
                titleUpdated=frameStart; titleFrames=0;
            }
            if (fpsLimit>0) {
                const auto frameDuration=std::chrono::duration<double>(1.0/fpsLimit);
                std::this_thread::sleep_until(frameStart+std::chrono::duration_cast<Clock::duration>(frameDuration));
            }
        }
        if (!config.screenshot.empty() && frames>0) renderer.screenshot(config.screenshot);
        glFinish();
        if (glGetError()!=GL_NO_ERROR) throw std::runtime_error("OpenGL-piirtovirhe");
        const double elapsed=std::chrono::duration<double>(Clock::now()-began).count();
        std::cout << frames << " ruutua / " << elapsed << " s";
        if (elapsed>0) std::cout << " = " << frames/elapsed << " FPS (sisältää mahdollisen kuvakaappauksen)";
        std::cout << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "PsyTunnel: " << error.what() << '\n';
        return 1;
    }
}
