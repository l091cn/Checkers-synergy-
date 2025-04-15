#pragma once
#include <chrono>
#include <thread>

#include "../Models/Project_path.h"
#include "Board.h"
#include "Config.h"
#include "Hand.h"
#include "Logic.h"

class Game
{
public:
    Game() : board(config("WindowSize", "Width"), config("WindowSize", "Hight")), hand(&board), logic(&board, &config)
    {
        ofstream fout(project_path + "log.txt", ios_base::trunc);
        fout.close();
    }

    // Функция play() запускает и управляет игровым процессом шашек
    int play()
    {
        auto start = chrono::steady_clock::now();
        if (is_replay)
        {
            logic = Logic(&board, &config);
            config.reload();
            board.redraw();
        }
        else
        {
            board.start_draw();
        }
        is_replay = false;

        int turn_num = -1;
        bool is_quit = false;
        const int Max_turns = config("Game", "MaxNumTurns");
        while (++turn_num < Max_turns)  // Цикл игры, продолжается до достижения максимального числа ходов
        {
            beat_series = 0;  // Сброс счетчика серии взятий
            logic.find_turns(turn_num % 2);  // Поиск возможных ходов для текущего игрока (0 - белые, 1 - черные)
            if (logic.turns.empty())  // Проверка на окончание игры - если ходов нет, игра завершается
                break;
            // Определение уровня сложности бота для текущего игрока
            logic.Max_depth = config("Bot", string((turn_num % 2) ? "Black" : "White") + string("BotLevel"));
            // Проверка, кто сейчас ходит - игрок или бот
            if (!config("Bot", string("Is") + string((turn_num % 2) ? "Black" : "White") + string("Bot")))
            {
                // Обработка хода игрока
                auto resp = player_turn(turn_num % 2);
                if (resp == Response::QUIT)  // Если игрок решил выйти
                {
                    is_quit = true;
                    break;
                }
                else if (resp == Response::REPLAY)  // Если игрок решил начать заново
                {
                    is_replay = true;
                    break;
                }
                else if (resp == Response::BACK)  // Если игрок решил отменить ход
                {
                    // Логика отмены ходов с учетом типа противника (бот/игрок)
                    if (config("Bot", string("Is") + string((1 - turn_num % 2) ? "Black" : "White") + string("Bot")) &&
                        !beat_series && board.history_mtx.size() > 2)
                    {
                        board.rollback();
                        --turn_num;
                    }
                    if (!beat_series)
                        --turn_num;

                    board.rollback();
                    --turn_num;
                    beat_series = 0;
                }
            }
            else
                bot_turn(turn_num % 2);  // Обработка хода бота
        }
        // Запись времени игры в лог
        auto end = chrono::steady_clock::now();
        ofstream fout(project_path + "log.txt", ios_base::app);
        fout << "Game time: " << (int)chrono::duration<double, milli>(end - start).count() << " millisec\n";
        fout.close();

        if (is_replay)  // Если запрошена новая игра, рекурсивно вызываем play()
            return play();
        if (is_quit)  // Если игра завершена выходом, возвращаем 0
            return 0;
        // Определение результата игры
        int res = 2;  // По умолчанию - ничья
        if (turn_num == Max_turns)
        {
            res = 0;  // Ничья по времени
        }
        else if (turn_num % 2)
        {
            res = 1;  // Победа белых
        }
        // Иначе победа черных (res = 2)

        board.show_final(res);  // Отображение финального результата

        // Ожидание действия игрока после окончания игры
        auto resp = hand.wait();
        if (resp == Response::REPLAY)
        {
            is_replay = true;
            return play();  // Начать новую игру
        }
        return res;  // Вернуть результат игры
    }

private:
    void bot_turn(const bool color)
    {
        auto start = chrono::steady_clock::now();

        auto delay_ms = config("Bot", "BotDelayMS");
        // new thread for equal delay for each turn
        thread th(SDL_Delay, delay_ms);
        auto turns = logic.find_best_turns(color);
        th.join();
        bool is_first = true;
        // making moves
        for (auto turn : turns)
        {
            if (!is_first)
            {
                SDL_Delay(delay_ms);
            }
            is_first = false;
            beat_series += (turn.xb != -1);
            board.move_piece(turn, beat_series);
        }

        auto end = chrono::steady_clock::now();
        ofstream fout(project_path + "log.txt", ios_base::app);
        fout << "Bot turn time: " << (int)chrono::duration<double, milli>(end - start).count() << " millisec\n";
        fout.close();
    }
    // Функция player_turn() обрабатывает ход игрока указанного цвета (color: 0 - белые, 1 - черные)
    // Возвращает Response с результатом действия игрока
    Response player_turn(const bool color)
    {
        // Создаем список клеток, содержащих фигуры, которыми можно ходить
        vector<pair<POS_T, POS_T>> cells;
        for (auto turn : logic.turns)
        {
            cells.emplace_back(turn.x, turn.y);
        }
        board.highlight_cells(cells);  // Подсвечиваем эти клетки на доске
        move_pos pos = { -1, -1, -1, -1 };  // Инициализация структуры хода
        POS_T x = -1, y = -1;  // Координаты выбранной фигуры
        // Цикл для выбора фигуры и получения первого хода
        while (true)
        {
            auto resp = hand.get_cell();  // Ожидание выбора клетки игроком
            if (get<0>(resp) != Response::CELL)  // Если получен не выбор клетки, а другое действие
                return get<0>(resp);  // Возвращаем это действие (QUIT, REPLAY, BACK)
            pair<POS_T, POS_T> cell{ get<1>(resp), get<2>(resp) };  // Получаем координаты выбранной клетки

            bool is_correct = false;  // Флаг корректности выбранной клетки
            // Проверяем, является ли выбор корректным (начало хода или конец хода)
            for (auto turn : logic.turns)
            {
                if (turn.x == cell.first && turn.y == cell.second)  // Если выбрана клетка с фигурой, которой можно ходить
                {
                    is_correct = true;
                    break;
                }
                if (turn == move_pos{ x, y, cell.first, cell.second })  // Если уже выбрана фигура и это выбор куда ходить
                {
                    pos = turn;  // Запоминаем полностью ход
                    break;
                }
            }
            if (pos.x != -1)  // Если ход полностью определен
                break;
            if (!is_correct)  // Если выбрана некорректная клетка
            {
                if (x != -1)  // Если фигура уже была выбрана, сбрасываем выбор
                {
                    board.clear_active();  // Убираем активную фигуру
                    board.clear_highlight();  // Убираем подсветку возможных ходов
                    board.highlight_cells(cells);  // Снова подсвечиваем фигуры, которыми можно ходить
                }
                x = -1;
                y = -1;
                continue;
            }
            // Запоминаем координаты выбранной фигуры
            x = cell.first;
            y = cell.second;
            board.clear_highlight();  // Убираем предыдущую подсветку
            board.set_active(x, y);  // Помечаем выбранную фигуру как активную
            // Подсвечиваем клетки, куда можно пойти выбранной фигурой
            vector<pair<POS_T, POS_T>> cells2;
            for (auto turn : logic.turns)
            {
                if (turn.x == x && turn.y == y)
                {
                    cells2.emplace_back(turn.x2, turn.y2);
                }
            }
            board.highlight_cells(cells2);
        }
        board.clear_highlight();  // Убираем подсветку
        board.clear_active();  // Убираем активную фигуру
        board.move_piece(pos, pos.xb != -1);  // Выполняем ход (с побитием или без)
        if (pos.xb == -1)  // Если не было взятия фигуры
            return Response::OK;  // Ход завершен

        // Обработка серии взятий (если есть возможность продолжить бить)
        beat_series = 1;  // Счетчик серии взятий
        while (true)
        {
            logic.find_turns(pos.x2, pos.y2);  // Ищем возможные ходы с новой позиции
            if (!logic.have_beats)  // Если нет возможности взятия, завершаем серию
                break;

            // Подсвечиваем клетки, куда можно сделать следующий ход в серии взятий
            vector<pair<POS_T, POS_T>> cells;
            for (auto turn : logic.turns)
            {
                cells.emplace_back(turn.x2, turn.y2);
            }
            board.highlight_cells(cells);
            board.set_active(pos.x2, pos.y2);  // Помечаем текущую позицию фигуры как активную
            // Цикл для выбора следующего хода в серии взятий
            while (true)
            {
                auto resp = hand.get_cell();  // Ожидание выбора клетки игроком
                if (get<0>(resp) != Response::CELL)  // Если получен не выбор клетки, а другое действие
                    return get<0>(resp);  // Возвращаем это действие (QUIT, REPLAY, BACK)
                pair<POS_T, POS_T> cell{ get<1>(resp), get<2>(resp) };  // Получаем координаты выбранной клетки

                bool is_correct = false;  // Флаг корректности выбранной клетки
                // Проверяем, является ли выбор корректным (куда можно сходить)
                for (auto turn : logic.turns)
                {
                    if (turn.x2 == cell.first && turn.y2 == cell.second)  // Если выбрана клетка, куда можно сходить
                    {
                        is_correct = true;
                        pos = turn;  // Запоминаем ход
                        break;
                    }
                }
                if (!is_correct)  // Если выбрана некорректная клетка
                    continue;  // Ждем правильного выбора

                board.clear_highlight();  // Убираем подсветку
                board.clear_active();  // Убираем активную фигуру
                beat_series += 1;  // Увеличиваем счетчик серии взятий
                board.move_piece(pos, beat_series);  // Выполняем ход
                break;
            }
        }

        return Response::OK;  // Ход успешно завершен
    }

private:
    Config config;
    Board board;
    Hand hand;
    Logic logic;
    int beat_series;
    bool is_replay = false;
};
