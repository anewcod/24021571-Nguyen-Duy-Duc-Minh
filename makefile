all:
	g++ -Iinclude -Iinclude/sdl -Iinclude/headers -Llib -o Main src/main.cpp -lmingw32 -lSDL2main -lSDL2 -lSDL2_image -lSDL2_ttf -lSDL2_mixer