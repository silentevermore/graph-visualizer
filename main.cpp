// DEFINITIONS
#include <SDL3/SDL_blendmode.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_stdinc.h>
#include <cstddef>
#include <string>
#define SDL_MAIN_USE_CALLBACKS 1
#define DEFAULT_FONT_SIZE 18
#define MAIN_FONT "fonts/AdwaitaSans-Regular.ttf"

// INCLUDES
#include<vector>
#include<iostream>
#include<cmath>

#include<SDL3/SDL.h>
#include<SDL3/SDL_main.h>
#include<SDL3_ttf/SDL_ttf.h>
#include<SDL3_image/SDL_image.h>

#include<muParser.h>

#include<imgui.h>
#include<imgui_impl_sdl3.h>
#include<imgui_impl_sdlrenderer3.h>
#include<imgui_stdlib.h>

// NAMESPACES
using namespace std;

// STRUCTS
struct FuncObject{
    float* color_4f;
    mu::Parser parser;
    string expr;
    bool is_expr_valid=false;
};

// CONSTANTS
vector<FuncObject> FUNCTIONS_LIST;
double GLOBAL_X=NAN;
int WINDOW_WIDTH=1280;
int WINDOW_HEIGHT=720;
const float MIN_ZOOM=.005, MAX_ZOOM=1, ZOOM_INCREMENT=.01;

// VARIABLES
SDL_Point GRAPH_POS={WINDOW_WIDTH/2, WINDOW_HEIGHT/2};
int FOCUSED_FUNCOBJ=-1;
bool CAN_DRAG_GRAPH=true;
float GRAPH_SCALE=.05;
float mPosX=0, mPosY=0;
bool mouse1=false, mouse2=false;
uint64_t lastTime=SDL_GetPerformanceCounter();
uint64_t fpsTimer=lastTime;
uint64_t freq=SDL_GetPerformanceFrequency();
int frames=0;
int fps=0;

// SDL3
static SDL_Window* win;
static SDL_Renderer* rendd;
float* TEXT_COLOR;
float* BG_COLOR;
TTF_Font* TEXT_FONT;

// SIGN NUMBER
template <typename T> int sgn(T val) {
    return (T(0) < val) - (val < T(0));
}

// GRID SNAPPING
double snapToGrid(double x, double g=.5){
    return round(x/g)*g;
}

// VALIDITY CHECK
void validateExpr(FuncObject &func){
    try{
        func.parser.Eval();
        func.is_expr_valid=true;
    }catch(mu::Parser::exception_type &e){
        func.is_expr_valid=false;
    }
}

// PURE-EVALUATING EXPRESSION
double pureEvalExpr(FuncObject &func, double x){
    GLOBAL_X=x;
    if (isnan(GLOBAL_X) || !func.is_expr_valid) return NAN;
    try{
        return func.parser.Eval();
    }catch(mu::Parser::exception_type &e){
        return NAN;
    }
}

// EVALUATING EXPRESSION
double evalExpr(FuncObject &func, double x){
    return -pureEvalExpr(func, x*GRAPH_SCALE)/GRAPH_SCALE;
}

// ADD NEW FUNCTION
FuncObject createFunc(string expression, float* color=nullptr){
    static float def_color[]{1,1,1,1};
    FuncObject a{};
    color=(color!=nullptr) ? color : def_color;
    a.color_4f=color;
    a.expr=expression;
    a.parser.DefineVar("x", &GLOBAL_X);
    a.parser.SetExpr(expression);
    validateExpr(a);
    return a;
}

// CALCULATES COLOR BY LUMINANCE VALUE
float* calculateColorOffset(float* col, float offset){
    float luminance = (0.2126f * col[0] + 0.7152f * col[2] + 0.0722f * col[3]);
    
    static float grid[4];

    if (luminance > 0.5f) {
        grid[0] = max<float>(0, col[0] - offset);
        grid[1] = max<float>(0, col[1] - offset);
        grid[2] = max<float>(0, col[2] - offset);
        grid[3] = col[3];
    } else {
        grid[0] = min<float>(255, col[0] + offset);
        grid[1] = min<float>(255, col[1] + offset);
        grid[2] = min<float>(255, col[2] + offset);
    }
    grid[3] = col[3];

    return grid;
}

// CONVERTS FLOAT[4] COLOR TO SDL_COLOR
SDL_Color toSDLCol(float col[4]){
    return SDL_Color{(Uint8)(col[0]*255.0f), (Uint8)(col[1]*255.0f), (Uint8)(col[2]*255.0f), (Uint8)(col[3]*255.0f)};
}

// TEXT DRAW
void drawText(SDL_Renderer* rend, TTF_Font* font, float textColor[4], float* bgColor=nullptr, SDL_Point pos={0, 0}, string text=" ", SDL_Point offsetInfluence={0, 0}){
    SDL_Color bg_col={0,0,0,0};
    if (bgColor!=nullptr) bg_col=toSDLCol(bgColor);
    SDL_Surface *textSurface=TTF_RenderText_Solid(font, text.c_str(), 0, toSDLCol(textColor));
    SDL_Texture *textTexture=SDL_CreateTextureFromSurface(rend, textSurface);
    SDL_FRect textRect = {static_cast<float>(pos.x + textSurface->w*offsetInfluence.x), static_cast<float>(pos.y + textSurface->h*offsetInfluence.y), static_cast<float>(textSurface->w), static_cast<float>(textSurface->h)};
	SDL_SetRenderDrawColor(rend, bg_col.r, bg_col.g, bg_col.b, bg_col.a);
    SDL_RenderFillRect(rend, &textRect);
    SDL_RenderTexture(rend, textTexture, NULL, &textRect);
    SDL_DestroyTexture(textTexture);
    SDL_DestroySurface(textSurface);
}

// DRAWS GRID
void drawGrid(SDL_Renderer* rend, SDL_Point origin, float scale, int width, int height){
    float* grid_color=calculateColorOffset(BG_COLOR, 40);
    float step = 1.0f / scale;
    TEXT_COLOR=grid_color;

    while (step < 20) step *= 5; 
    while (step > 200) step /= 5;

    float startX = fmod(origin.x, step);
    for (float x = startX; x < width; x += step){
        drawText(rendd, TEXT_FONT, TEXT_COLOR, nullptr, {(int)x, GRAPH_POS.y}, to_string((int)((x - GRAPH_POS.x)*GRAPH_SCALE)));
        SDL_SetRenderDrawColor(rendd, grid_color[0]*255, grid_color[1]*255, grid_color[2]*255, 50);
        SDL_RenderLine(rend, x, 0, x, height);
    }

    float startY = fmod(origin.y, step);
    for (float y = startY; y < height; y += step){
        SDL_RenderLine(rend, 0, y, width, y);
    }
}

// INITIALIZATION
SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]){
    static float bg_col[4]={90.0/255, 90.0/255, 90.0/255, 1};
    static float tex_col[4]={1, 1, 1, 1};
    BG_COLOR=bg_col;
    TEXT_COLOR=tex_col;

	SDL_Init(SDL_INIT_VIDEO);
	SDL_Init(SDL_INIT_EVENTS);
	TTF_Init();
	SDL_CreateWindowAndRenderer("Graph Renderer", WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_RESIZABLE, &win, &rendd);
	TEXT_FONT=TTF_OpenFont(MAIN_FONT, DEFAULT_FONT_SIZE);
    // SDL_SetRenderVSync(rendd, 1);

    float main_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsClassic();

    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);
    style.FontScaleDpi = main_scale;

    ImGui_ImplSDL3_InitForSDLRenderer(win, rendd);
    ImGui_ImplSDLRenderer3_Init(rendd);

    SDL_Surface* icon=IMG_Load("resources/icon.png");
    if (icon){
        SDL_SetWindowIcon(win, icon);
        SDL_DestroySurface(icon);
    }

    SDL_StartTextInput(win);
	return SDL_APP_CONTINUE;
}

// MAIN
void do_shit(){
	//assign
	SDL_GetWindowSize(win, &WINDOW_WIDTH, &WINDOW_HEIGHT);

	//cleanup
	SDL_SetRenderDrawColor(rendd, BG_COLOR[0]*255, BG_COLOR[1]*255, BG_COLOR[2]*255, BG_COLOR[3]*255);
	SDL_RenderClear(rendd);
    SDL_SetRenderDrawBlendMode(rendd, SDL_BLENDMODE_BLEND);

    //render grid and center lines
    float* center_lines_color=calculateColorOffset(BG_COLOR, 70);

    drawGrid(rendd, GRAPH_POS, GRAPH_SCALE, WINDOW_WIDTH, WINDOW_HEIGHT);

    SDL_SetRenderDrawColor(rendd, center_lines_color[0]*255, center_lines_color[1]*255, center_lines_color[2]*255, 100);
    SDL_RenderLine(rendd, GRAPH_POS.x, 0, GRAPH_POS.x, WINDOW_HEIGHT);
    SDL_RenderLine(rendd, 0, GRAPH_POS.y, WINDOW_WIDTH, GRAPH_POS.y);

    //imgui
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowSize(ImVec2(WINDOW_WIDTH * 1/5, WINDOW_HEIGHT));
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::Begin("Functions", (bool*)false, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
    
    if (ImGui::BeginMenu("Settings")){
        if (ImGui::ColorPicker4("Background", BG_COLOR)){}
        ImGui::End();
    }

    if (ImGui::Button("Reset position"))
        GRAPH_POS={WINDOW_WIDTH/2, WINDOW_HEIGHT/2};
    ImGui::SameLine();
    if (ImGui::Button("Clear all")) FUNCTIONS_LIST.clear();
    ImGui::SameLine();
    if (ImGui::Button("+"))
        FUNCTIONS_LIST.push_back(createFunc(""));

    //parse all function objects
    for (int i=0; i<FUNCTIONS_LIST.size(); i++){
        FuncObject& func=FUNCTIONS_LIST.at(i);

        //do imgui shit
        const char* name=("##" + to_string(i)).c_str();
        ImGui::PushID(name);
        if (ImGui::InputText(name, &func.expr)){
            func.parser.SetExpr(func.expr);
            validateExpr(func);
        }
        ImGui::SameLine();
        if (ImGui::Button("-"))
            FUNCTIONS_LIST.erase(FUNCTIONS_LIST.begin() + i);
        ImGui::SameLine();
        if (ImGui::ColorButton("##", ImVec4(func.color_4f[0], func.color_4f[1], func.color_4f[2], func.color_4f[3])))
            FOCUSED_FUNCOBJ=(FOCUSED_FUNCOBJ==i ? -1 : i);
        
        ImGui::PopID();

        //do calculation and render
        double lastX=NAN, lastY=NAN;
        SDL_SetRenderDrawColor(rendd, func.color_4f[0]*255, func.color_4f[1]*255, func.color_4f[2]*255, func.color_4f[3]*255);
        for (double x=-GRAPH_POS.x; x<WINDOW_WIDTH-GRAPH_POS.x; x++){
            double y=evalExpr(func, x);
            double newX=x + GRAPH_POS.x;
            double newY=y + GRAPH_POS.y;
            if (!isnan(newY) && !isnan(lastY) && !isinf(newY) && !isinf(lastY))
                SDL_RenderLine(rendd, lastX, lastY, newX, newY);
            lastX=newX;
            lastY=newY;
        }

        if (FOCUSED_FUNCOBJ==i){
            ImGui::SetNextWindowSize(ImVec2(256, 256));
            ImGui::SetNextWindowPos(ImVec2(WINDOW_WIDTH * 1/5 + 10, 0));
            ImGui::Begin("Colors", (bool*)false, ImGuiWindowFlags_NoResize);

            ImGui::ColorPicker4("##", func.color_4f);

            ImGui::End();
        }
    }
    ImGui::End();

    //render intersection coordinates
    if (FOCUSED_FUNCOBJ!=-1 && !FUNCTIONS_LIST.empty() && mouse2){
        FuncObject& func=FUNCTIONS_LIST.at(FOCUSED_FUNCOBJ);
        double x=mPosX;
        double y=pureEvalExpr(func, (x - GRAPH_POS.x)*GRAPH_SCALE);
        double newY=evalExpr(func, x - GRAPH_POS.x) + GRAPH_POS.y;
        SDL_SetRenderDrawColor(rendd, 100, 255, 100, 255);
        SDL_RenderLine(rendd, 0, newY, WINDOW_WIDTH, newY);
        SDL_RenderLine(rendd, x, 0, x, WINDOW_HEIGHT);
        drawText(rendd, TEXT_FONT, TEXT_COLOR, BG_COLOR, {(int)mPosX, (int)mPosY}, to_string((x - GRAPH_POS.x)*GRAPH_SCALE) + ", " + to_string(y), {0, -1});
    }

	//rendering
    CAN_DRAG_GRAPH=!(ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) || ImGui::IsAnyItemActive());
    ImGui::Render();
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), rendd);
    SDL_RenderPresent(rendd);
}

SDL_AppResult SDL_AppIterate(void* appstate){
    do_shit();
    return SDL_APP_CONTINUE;
}

// EVENT HANDLING
SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event){
    ImGui_ImplSDL3_ProcessEvent(event);
	switch (event->type){
		case SDL_EVENT_QUIT:
			return SDL_APP_SUCCESS;
		case SDL_EVENT_MOUSE_WHEEL:
            if (!CAN_DRAG_GRAPH) return SDL_APP_CONTINUE;
            GRAPH_SCALE=clamp<float>(GRAPH_SCALE - event->wheel.y*ZOOM_INCREMENT, MIN_ZOOM, MAX_ZOOM);
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (event->button.button==1){
                mouse1=event->button.down;
            }else if(event->button.button==3){
                mouse2=event->button.down;
            }
            break;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (event->button.button==1){
                mouse1=event->button.down;
            }else if(event->button.button==3){
                mouse2=event->button.down;
            }
            break;
        case SDL_EVENT_MOUSE_MOTION:
            if (!CAN_DRAG_GRAPH) return SDL_APP_CONTINUE;
            if (mouse1 && event->motion.state==1){
                GRAPH_POS.x+=event->motion.xrel;
                GRAPH_POS.y+=event->motion.yrel;
            }
            mPosX=event->motion.x;
            mPosY=event->motion.y;
            break;
	}
	return SDL_APP_CONTINUE;
}

// QUIT
void SDL_AppQuit(void* appstate, SDL_AppResult result){
	TTF_CloseFont(TEXT_FONT);
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
}