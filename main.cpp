// DEFINITIONS
#define SDL_MAIN_USE_CALLBACKS 1
#define DEFAULT_FONT_SIZE 24
#define MAIN_FONT "fonts/Mono-Regular.ttf"

// INCLUDES
#include<SDL3/SDL.h>
#include<SDL3/SDL_main.h>
#include<SDL3_ttf/SDL_ttf.h>
#include<muparser/muParser.h>
#include<iostream>

// NAMESPACES
using namespace std;

//CONSTANTS
int WINDOW_WIDTH=1280;
int WINDOW_HEIGHT=720;
const float MIN_ZOOM=.005, MAX_ZOOM=1, ZOOM_INCREMENT=.01;

// VARIABLES
SDL_Point GRAPH_POS={WINDOW_WIDTH/2, WINDOW_HEIGHT/2};
string EXPRESSION="";
float GRAPH_SCALE=.05;
float mPosX=0, mPosY=0;
bool mouse1=false, mouse2=false;
bool LCTRL_DOWN=false;
uint64_t lastTime=SDL_GetPerformanceCounter();
uint64_t fpsTimer=lastTime;
uint64_t freq=SDL_GetPerformanceFrequency();
int frames=0;
int fps=0;

// SDL3
static SDL_Window* win;
static SDL_Renderer* rendd;
SDL_Color TEXT_COLOR={255, 255, 255, 255};
SDL_Color BG_COLOR={30, 30, 30, 255};
TTF_Font* TEXT_FONT;

// MUPARSER
mu::Parser parser;
double GLOBAL_X=NAN;
bool IS_EXPRESSION_VALID=false;

// VALIDITY CHECK
void validateExpr(){
    try{
        parser.Eval();
        IS_EXPRESSION_VALID=true;
    }catch(mu::Parser::exception_type &e){
        IS_EXPRESSION_VALID=false;
    }
}

// EVALUATING EXPRESSION
double evalExpr(double x){
    GLOBAL_X=x*GRAPH_SCALE;
    if (isnan(GLOBAL_X) || !IS_EXPRESSION_VALID) return NAN;
    try{
        return -parser.Eval()/GRAPH_SCALE;
    }catch(mu::Parser::exception_type &e){
        return NAN;
    }
}

// C-STRING CONVERSION
const char* strToC(string a){
    return (!a.empty() ? a.c_str() : " ");
}

// TEXT DRAW
void drawText(SDL_Renderer* rend, TTF_Font* font, SDL_Color textColor, SDL_Color bgColor, SDL_Point pos, string text, SDL_Point offsetInfluence={0, 0}){
    SDL_Surface *textSurface=TTF_RenderText_Solid(font, strToC(text), 0, textColor);
    SDL_Texture *textTexture=SDL_CreateTextureFromSurface(rend, textSurface);
    SDL_FRect textRect = {(int)(pos.x + textSurface->w*offsetInfluence.x), (int)(pos.y + textSurface->h*offsetInfluence.y), textSurface->w, textSurface->h};
	SDL_SetRenderDrawColor(rend, bgColor.r, bgColor.g, bgColor.b, bgColor.a);
    SDL_RenderFillRect(rend, &textRect);
    SDL_RenderTexture(rend, textTexture, NULL, &textRect);
}

// INITIALIZATION
SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]){
	SDL_Init(SDL_INIT_VIDEO);
	SDL_Init(SDL_INIT_EVENTS);
	TTF_Init();
	SDL_CreateWindowAndRenderer("Graph Renderer", WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL, &win, &rendd);
	TEXT_FONT=TTF_OpenFont(MAIN_FONT, DEFAULT_FONT_SIZE);
    parser.DefineVar("x", &GLOBAL_X);
	parser.SetExpr(EXPRESSION);
    validateExpr();
    SDL_StartTextInput(win);
	return SDL_APP_CONTINUE;
}

// MAIN
SDL_AppResult SDL_AppIterate(void* appstate){
	//assign
	SDL_GetWindowSize(win, &WINDOW_WIDTH, &WINDOW_HEIGHT);
	//cleanup
	SDL_SetRenderDrawColor(rendd, BG_COLOR.r, BG_COLOR.g, BG_COLOR.b, BG_COLOR.a);
	SDL_RenderClear(rendd);
	//set blendmode
    SDL_SetRenderDrawBlendMode(rendd, SDL_BLENDMODE_BLEND);
    //render center lines
    SDL_SetRenderDrawColor(rendd, 155, 100, 255, 155);
    SDL_RenderLine(rendd, GRAPH_POS.x, 0, GRAPH_POS.x, WINDOW_HEIGHT);
    SDL_RenderLine(rendd, 0, GRAPH_POS.y, WINDOW_WIDTH, GRAPH_POS.y);
    //render expression
    float lastX=NAN, lastY=NAN;
    SDL_SetRenderDrawColor(rendd, 200, 200, 200, 255);
    for (float x=-GRAPH_POS.x; x<WINDOW_WIDTH-GRAPH_POS.x; x++){
        float y=evalExpr(x);
        float newX=x + GRAPH_POS.x;
        float newY=y + GRAPH_POS.y;
        if (!isnan(newY) && !isnan(lastY)){
            SDL_RenderLine(rendd, lastX, lastY, newX, newY);
        }
        lastX=newX;
        lastY=newY;
    }
    //render intersection
    if (mouse2){
        SDL_SetRenderDrawColor(rendd, 255, 155, 155, SDL_ALPHA_OPAQUE);
        SDL_RenderLine(rendd, mPosX, 0, mPosX, WINDOW_HEIGHT);
        float localX=mPosX - GRAPH_POS.x;
        float localY=evalExpr(localX) + GRAPH_POS.y;
        if (!isnan(localY)){
            SDL_RenderLine(rendd, 0, localY, WINDOW_WIDTH, localY);
        }
    }
    //render expression text
    drawText(rendd, TEXT_FONT, TEXT_COLOR, BG_COLOR, {0, 0}, EXPRESSION+(fmod(SDL_GetTicks(), 1000)/500>=1 ? "|" : ""));
    //calculate and render FPS value
    frames++;
    uint64_t now=SDL_GetPerformanceCounter();
    if ((now-fpsTimer)>=freq){
        fps=frames;
        frames=0;
        fpsTimer=now;
    }
    drawText(rendd, TEXT_FONT, TEXT_COLOR, BG_COLOR, {WINDOW_WIDTH, 0}, "FPS: "+to_string(fps), {-1, 0});
    //render intersection coords
    if (mouse2) drawText(rendd, TEXT_FONT, TEXT_COLOR, BG_COLOR, SDL_Point{(int)mPosX+24, (int)mPosY}, 
    "(" + to_string(fmod(mPosX - GRAPH_POS.x, WINDOW_WIDTH)*GRAPH_SCALE) + ", " + to_string(-evalExpr(mPosX - GRAPH_POS.x)*GRAPH_SCALE) + ")");
	//rendering
	SDL_RenderPresent(rendd);
	return SDL_APP_CONTINUE;
}

// EVENT HANDLING
SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event){
	switch (event->type){
		case SDL_EVENT_QUIT:
			return SDL_APP_SUCCESS;
        case SDL_EVENT_KEY_UP:
            LCTRL_DOWN=false;
            break;
		case SDL_EVENT_KEY_DOWN:
            if (event->key.scancode==SDL_SCANCODE_BACKSPACE){
                if (!EXPRESSION.empty())
                    (LCTRL_DOWN==true) ? EXPRESSION.clear() : EXPRESSION.pop_back();
                parser.SetExpr(EXPRESSION);
                validateExpr();
			}else if (event->key.scancode==SDL_SCANCODE_LCTRL){
                LCTRL_DOWN=true;
            }
			break;
		case SDL_EVENT_MOUSE_WHEEL:
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
            if (mouse1 && event->motion.state==1){
                GRAPH_POS.x+=event->motion.xrel;
                GRAPH_POS.y+=event->motion.yrel;
            }
            mPosX=event->motion.x;
            mPosY=event->motion.y;
            break;
    	case SDL_EVENT_TEXT_INPUT:
            EXPRESSION.append(event->text.text);
            parser.SetExpr(EXPRESSION);
            validateExpr();
            break;
	}
	return SDL_APP_CONTINUE;
}

// QUIT
void SDL_AppQuit(void* appstate, SDL_AppResult result){
	TTF_CloseFont(TEXT_FONT);
}