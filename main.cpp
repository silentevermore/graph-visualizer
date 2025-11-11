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

// VARIABLES
SDL_Point GRAPH_POS={0, 0};
float GRAPH_SCALE=.05;
mu::string_type EXPRESSION;

// SDL3
static SDL_Window* win;
static SDL_Renderer* rendd;
SDL_Color TEXT_COLOR={255, 255, 255, 255};
SDL_Color BG_COLOR={30, 30, 30, 255};
TTF_Font* TEXT_FONT;

// MUPARSER
mu::Parser parser;

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
	SDL_CreateWindowAndRenderer("Graph Renderer", WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_RESIZABLE, &win, &rendd);
	TEXT_FONT=TTF_OpenFont(MAIN_FONT, DEFAULT_FONT_SIZE);
	parser.SetExpr(EXPRESSION);
	return SDL_APP_CONTINUE;
}

// MAIN
SDL_AppResult SDL_AppIterate(void* appstate){
	//assign
	SDL_GetWindowSize(win, &WINDOW_WIDTH, &WINDOW_HEIGHT);
	//cleanup
	SDL_SetRenderDrawColor(rendd, BG_COLOR.r, BG_COLOR.g, BG_COLOR.b, BG_COLOR.a);
	SDL_RenderClear(rendd);
	//shit
	//rendering
	SDL_RenderPresent(rendd);
	return SDL_APP_CONTINUE;
}

// EVENT HANDLING
SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event){
	switch (event->type){
		case SDL_EVENT_QUIT:
			return SDL_APP_SUCCESS;
		case SDL_EVENT_KEY_DOWN:
			break;
	}
	return SDL_APP_CONTINUE;
}

// QUIT
void SDL_AppQuit(void* appstate, SDL_AppResult result){
	TTF_CloseFont(TEXT_FONT);
}