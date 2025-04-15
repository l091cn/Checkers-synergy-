#pragma once
#include <tuple>

#include "../Models/Move.h"
#include "../Models/Response.h"
#include "Board.h"

// Класс Hand отвечает за обработку пользовательского ввода и взаимодействие
// с игровой доской через события SDL (клики мыши, изменение размера окна и т.д.)
class Hand
{
  public:
    Hand(Board *board) : board(board)
    {
    }
    // Функция get_cell() ожидает выбора клетки пользователем и возвращает тип реакции
    // и координаты выбранной клетки (если таковая имеется)
    // Возвращает:
    // - Response::QUIT - если пользователь закрыл окно
    // - Response::BACK - если пользователь нажал кнопку отмены хода (левый верхний угол)
    // - Response::REPLAY - если пользователь нажал кнопку перезапуска (правый верхний угол)
    // - Response::CELL - если пользователь выбрал клетку на доске (с координатами xc, yc)
    tuple<Response, POS_T, POS_T> get_cell() const
    {
        SDL_Event windowEvent;
        Response resp = Response::OK;
        int x = -1, y = -1;
        int xc = -1, yc = -1;
        while (true)
        {
            if (SDL_PollEvent(&windowEvent))
            {
                switch (windowEvent.type)
                {
                case SDL_QUIT:
                    resp = Response::QUIT;  // Пользователь закрыл окно
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    // Обработка клика мыши - определение координат и действия
                    x = windowEvent.motion.x;
                    y = windowEvent.motion.y;
                    xc = int(y / (board->H / 10) - 1);  // Пересчет координат экрана в координаты доски
                    yc = int(x / (board->W / 10) - 1);
                    if (xc == -1 && yc == -1 && board->history_mtx.size() > 1)
                    {
                        resp = Response::BACK;  // Клик по кнопке "отмена хода"
                    }
                    else if (xc == -1 && yc == 8)
                    {
                        resp = Response::REPLAY;  // Клик по кнопке "перезапустить игру"
                    }
                    else if (xc >= 0 && xc < 8 && yc >= 0 && yc < 8)
                    {
                        resp = Response::CELL;  // Выбрана клетка на доске
                    }
                    else
                    {
                        xc = -1;  // Клик вне доски и кнопок - игнорируем
                        yc = -1;
                    }
                    break;
                case SDL_WINDOWEVENT:
                    // Обработка изменения размера окна
                    if (windowEvent.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                    {
                        board->reset_window_size();
                        break;
                    }
                }
                if (resp != Response::OK)
                    break;
            }
        }
        return {resp, xc, yc};
    }
    // Функция wait() ожидает любое действие пользователя (клик, закрытие окна)
    // Используется в конце игры или при ожидании решения пользователя
    // Возвращает:
    // - Response::QUIT - если пользователь закрыл окно
    // - Response::REPLAY - если пользователь нажал кнопку перезапуска
    Response wait() const
    {
        SDL_Event windowEvent;
        Response resp = Response::OK;
        while (true)
        {
            if (SDL_PollEvent(&windowEvent))
            {
                switch (windowEvent.type)
                {
                case SDL_QUIT:
                    resp = Response::QUIT;  // Пользователь закрыл окно
                    break;
                case SDL_WINDOWEVENT_SIZE_CHANGED:
                    board->reset_window_size();  // Обработка изменения размера окна
                    break;
                case SDL_MOUSEBUTTONDOWN: {
                    // Обработка клика мыши
                    int x = windowEvent.motion.x;
                    int y = windowEvent.motion.y;
                    int xc = int(y / (board->H / 10) - 1);
                    int yc = int(x / (board->W / 10) - 1);
                    if (xc == -1 && yc == 8)
                        resp = Response::REPLAY;  // Клик по кнопке "перезапустить игру"
                }
                break;
                }
                if (resp != Response::OK)
                    break;
            }
        }
        return resp;
    }

  private:
    Board *board;  // Указатель на игровую доску
};
