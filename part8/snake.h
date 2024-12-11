#include <ncurses.h>
#include <unistd.h>
#include <cstdlib>
#include <ctime>
#include <locale>
#include <vector>
#include <cwchar>
#include <sys/ioctl.h>

class SnakeGame {
public:
    SnakeGame() : gameOver(false), width(80), height(40), score(0), nTail(0), nObstacles(10), dir(STOP), fruitCounter(0), obstacleCounter(0) {
        obstaclesX.resize(nObstacles, -1);
        obstaclesY.resize(nObstacles, -1);
        tailX.resize(1024);
        tailY.resize(1024);
    }

    void Setup() {
        initscr();
        height = LINES - 1;
        width = COLS / 2 - 1;

        clear();
        noecho();
        cbreak();
        timeout(100);
        keypad(stdscr, TRUE);
        setlocale(LC_CTYPE, ""); // Enable UTF-8
        gameOver = false;
        dir = STOP;

        // Snake initial position
        x = width / 2;
        y = height / 2;

        // Fruit position
        fruitX = rand() % width;
        fruitY = rand() % height;

        score = 0;
        placeObstacles();
    }

    void Run() {
        while (!gameOver) {
            Draw();
            Input();
            Logic();
            usleep(100000); // Adjust speed
        }
        endwin();
    }

private:
    bool gameOver;
    int width, height, x, y, fruitX, fruitY, score, nTail, nObstacles;
    std::vector<int> obstaclesX, obstaclesY, tailX, tailY;
    enum Direction { STOP = 0, LEFT, RIGHT, UP, DOWN };
    Direction dir;
    int fruitCounter, obstacleCounter;

    wchar_t* randomFruit() {
        static wchar_t fruit[] = L"🍎🥝🍓🍋🍈🍍🥭🍉🍅🍇🍑🍒🍌🍏🫐";
        static wchar_t res[2] = L" ";
        res[0] = fruit[fruitCounter % 15];
        return res;
    }

    wchar_t* randomObstacle() {
        static wchar_t obstacles[] = L"🪵🪨🌊";
        static wchar_t res[2] = L" ";
        res[0] = obstacles[obstacleCounter++ % 3];
        return res;
    }

    void placeObstacles() {
        for (int i = 0; i < nObstacles; i++) {
            do {
                obstaclesX[i] = rand() % width;
            } while (obstaclesX[i] == fruitX);
            do {
                obstaclesY[i] = rand() % height;
            } while (obstaclesY[i] == fruitY);

            bool alreadyUsed = false;
            for (int j = 0; j < i; j++) {
                if (obstaclesX[i] == obstaclesX[j] && obstaclesY[i] == obstaclesY[j]) {
                    alreadyUsed = true;
                    break;
                }
            }
            if (alreadyUsed) {
                i--;
            }
        }
    }

    void Draw() {
        clear();
        wchar_t snakeHead[] = L"🐖";
        wchar_t snakeBody[] = L"🐖";
        obstacleCounter = 0;

        for (int i = 0; i < height; i++) {
            for (int j = 0; j < width; j++) {
                if (i == y && j == x)
                    addwstr(snakeHead); // Snake head
                else if (i == fruitY && j == fruitX)
                    addwstr(randomFruit()); // Fruit
                else {
                    bool print = false;
                    for (int k = 0; k < nTail; k++) {
                        if (tailX[k] == j && tailY[k] == i) {
                            addwstr(snakeBody); print = true; // Snake body
                            break;
                        }
                    }
                    for (int k = 0; k < nObstacles; k++) {
                        if (j == obstaclesX[k] && i == obstaclesY[k]) {
                            addwstr(randomObstacle());
                            print = true;
                        }
                    }
                    if (!print) addwstr(L"🌿"); // Empty space
                }
            }
            addwstr(L"\n");
        }
        addwstr(L"🐷");

        mvprintw(height, 3, "Score: %d", score);
        refresh();
    }

    void Input() {
        int c = getch();
        switch (c) {
        case KEY_LEFT:
            if (dir != RIGHT) dir = LEFT;
            break;
        case KEY_RIGHT:
            if (dir != LEFT) dir = RIGHT;
            break;
        case KEY_UP:
            if (dir != DOWN) dir = UP;
            break;
        case KEY_DOWN:
            if (dir != UP) dir = DOWN;
            break;
        case 'x':
            gameOver = true;
            break;
        }
    }

    void Logic() {
        int prevX = tailX[0], prevY = tailY[0], prev2X, prev2Y;
        tailX[0] = x;
        tailY[0] = y;

        for (int i = 1; i < nTail; i++) {
            prev2X = tailX[i];
            prev2Y = tailY[i];
            tailX[i] = prevX;
            tailY[i] = prevY;
            prevX = prev2X;
            prevY = prev2Y;
        }

        switch (dir) {
        case LEFT:  x--; break;
        case RIGHT: x++; break;
        case UP:    y--; break;
        case DOWN:  y++; break;
        default:    break;
        }

        if (x >= width) x = 0; else if (x < 0) x = width - 1;
        if (y >= height) y = 0; else if (y < 0) y = height - 1;

        for (int i = 0; i < nTail; i++)
            if (tailX[i] == x && tailY[i] == y) gameOver = true;

        for (int i = 0; i < nObstacles; i++)
            if (obstaclesX[i] == x && obstaclesY[i] == y) gameOver = true;

        if (x == fruitX && y == fruitY) {
            score += 10;
            nTail++;
            fruitCounter++;

            bool occupied = false;
            do {
                fruitX = rand() % width;
                fruitY = rand() % height;

                occupied = false;
                for (int i = 0; i < nObstacles; i++) {
                    if (obstaclesX[i] == fruitX || obstaclesY[i] == fruitY) {
                        occupied = true;
                        break;
                    }
                }
            } while (occupied);
        }
    }
};

