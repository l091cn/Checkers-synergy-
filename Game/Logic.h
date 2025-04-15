#pragma once
#include <random>
#include <vector>

#include "../Models/Move.h"
#include "Board.h"
#include "Config.h"

const int INF = 1e9;

class Logic
{
  public:
    Logic(Board *board, Config *config) : board(board), config(config)
    {
        rand_eng = std::default_random_engine (
            !((*config)("Bot", "NoRandom")) ? unsigned(time(0)) : 0);
        scoring_mode = (*config)("Bot", "BotScoringType");
        optimization = (*config)("Bot", "Optimization");
    }

    vector<move_pos> find_best_turns(const bool color)
    {
        next_best_state.clear();
        next_move.clear();

        find_first_best_turn(board->get_board(), color, -1, -1, 0);

        int cur_state = 0;
        vector<move_pos> res;
        do
        {
            res.push_back(next_move[cur_state]);
            cur_state = next_best_state[cur_state];
        } while (cur_state != -1 && next_move[cur_state].x != -1);
        return res;
    }

private:
    // Применяет ход turn к доске mtx и возвращает новое состояние доски.
    // Обрабатывает взятие фигуры, продвижение шашки в дамку и перемещение фигуры.
    vector<vector<POS_T>> make_turn(vector<vector<POS_T>> mtx, move_pos turn) const
    {
        if (turn.xb != -1)  // Если это ход со взятием
            mtx[turn.xb][turn.yb] = 0;  // Убираем побитую фигуру
        // Проверка на превращение в дамку (белая шашка дошла до верхнего края или черная до нижнего)
        if ((mtx[turn.x][turn.y] == 1 && turn.x2 == 0) || (mtx[turn.x][turn.y] == 2 && turn.x2 == 7))
            mtx[turn.x][turn.y] += 2;  // Превращаем шашку в дамку (+2 к значению)
        mtx[turn.x2][turn.y2] = mtx[turn.x][turn.y];  // Перемещаем фигуру на новую позицию
        mtx[turn.x][turn.y] = 0;  
        return mtx;
    }

    // Вычисляет оценку текущей позиции на доске для алгоритма минимакс.
    // Более высокая оценка соответствует более выгодной позиции для бота.
    // first_bot_color - цвет фигур бота (true - черные, false - белые).
    // Учитывает количество фигур, тип фигур (шашки и дамки) и, в зависимости от настроек,
    // их потенциал для продвижения (близость к краю доски).
    double calc_score(const vector<vector<POS_T>> &mtx, const bool first_bot_color) const
    {
        // color - who is max player
        double w = 0, wq = 0, b = 0, bq = 0;
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                w += (mtx[i][j] == 1);  // Количество белых шашек
                wq += (mtx[i][j] == 3);  // Количество белых дамок
                b += (mtx[i][j] == 2);  // Количество черных шашек
                bq += (mtx[i][j] == 4);  // Количество черных дамок
                if (scoring_mode == "NumberAndPotential")
                {
                    w += 0.05 * (mtx[i][j] == 1) * (7 - i);  // Потенциал белых шашек (близость к краю)
                    b += 0.05 * (mtx[i][j] == 2) * (i);  // Потенциал черных шашек (близость к краю)
                }
            }
        }
        if (!first_bot_color)  // Если бот играет за белых, меняем значения
        {
            swap(b, w);
            swap(bq, wq);
        }
        if (w + wq == 0)  // Если у противника нет фигур, это выигрыш
            return INF;
        if (b + bq == 0)  // Если у бота нет фигур, это проигрыш
            return 0;
        int q_coef = 4;  // Коэффициент значимости дамки по отношению к обычной шашке
        if (scoring_mode == "NumberAndPotential")
        {
            q_coef = 5;  // При учете потенциала дамки ценятся еще выше
        }
        return (b + bq * q_coef) / (w + wq * q_coef);  // Отношение силы бота к силе противника
    }

    // Функция для инициализации поиска наилучшего хода
    void find_best_turns(vector<vector<POS_T>> mtx, const bool color) {
        // Очистка предыдущих данных
        next_best_state.clear();
        next_move.clear();

        // Инициализация состояния
        size_t initial_state = 0;

        // Вызов рекурсивной функции поиска
        find_first_best_turn(mtx, color, -1, -1, initial_state);
    }

    // Функция для определения наилучшего хода на первом уровне поиска
    double find_first_best_turn(vector<vector<POS_T>> mtx, const bool color, const POS_T x, const POS_T y, size_t state, double alpha = -1) {
        // Добавление начального состояния
        next_best_state.push_back(-1);
        next_move.emplace_back(-1, -1, -1, -1);

        double best_score = -1;

        // Если указаны координаты, найти возможные ходы
        if (state != 0)
            find_turns(x, y, mtx);

        auto turns_now = turns;
        bool have_beats_now = have_beats;

        // Если нет обязательных взятий и не начальное состояние, перейти к следующему уровню
        if (!have_beats_now && state != 0) {
            return find_best_turns_rec(mtx, 1 - color, 0, alpha);
        }

        for (auto turn : turns_now) {
            size_t next_state = next_move.size();
            double score;

            if (have_beats_now) {
                // Рекурсивный вызов для продолжения взятий
                score = find_first_best_turn(make_turn(mtx, turn), color, turn.x2, turn.y2, next_state, best_score);
            }
            else {
                // Рекурсивный вызов для хода противника
                score = find_best_turns_rec(make_turn(mtx, turn), 1 - color, 0, best_score);
            }

            // Обновление наилучшего хода
            if (score > best_score) {
                best_score = score;
                next_best_state[state] = (have_beats_now ? int(next_state) : -1);
                next_move[state] = turn;
            }
        }

        return best_score;
    }

    // Рекурсивная функция минимакс с альфа-бета отсечением
    double find_best_turns_rec(vector<vector<POS_T>> mtx, const bool color, const size_t depth, double alpha = -1, double beta = INF + 1, const POS_T x = -1, const POS_T y = -1) {
        // Проверка на достижение максимальной глубины
        if (depth == Max_depth) {
            return calc_score(mtx, (depth % 2 == color));
        }

        // Определение возможных ходов
        if (x != -1) {
            find_turns(x, y, mtx);
        }
        else {
            find_turns(color, mtx);
        }

        auto turns_now = turns;
        bool have_beats_now = have_beats;

        // Если нет обязательных взятий и указаны координаты, перейти к следующему уровню
        if (!have_beats_now && x != -1) {
            return find_best_turns_rec(mtx, 1 - color, depth + 1, alpha, beta);
        }

        // Если нет возможных ходов, вернуть соответствующее значение
        if (turns.empty())
            return (depth % 2 ? 0 : INF);

        double min_score = INF + 1;
        double max_score = -1;

        for (auto turn : turns_now) {
            double score = 0.0;

            if (!have_beats_now && x == -1) {
                // Рекурсивный вызов для хода противника
                score = find_best_turns_rec(make_turn(mtx, turn), 1 - color, depth + 1, alpha, beta);
            }
            else {
                // Рекурсивный вызов для продолжения взятий
                score = find_best_turns_rec(make_turn(mtx, turn), color, depth, alpha, beta, turn.x2, turn.y2);
            }

            min_score = min(min_score, score);
            max_score = max(max_score, score);

            // Альфа-бета отсечение
            if (depth % 2)
                alpha = max(alpha, max_score);
            else
                beta = min(beta, min_score);

            if (optimization != "O0" && alpha >= beta)
                return (depth % 2 ? max_score + 1 : min_score - 1);
        }

        return (depth % 2 ? max_score : min_score);
    }


public:
    // Находит все возможные ходы для фигур указанного цвета (color: 0 - белые, 1 - черные)
    // на текущей доске и сохраняет их в вектор turns
    void find_turns(const bool color)
    {
        find_turns(color, board->get_board());
    }
    // Находит все возможные ходы для фигуры на позиции (x, y) на текущей доске
    // и сохраняет их в вектор turns
    void find_turns(const POS_T x, const POS_T y)
    {
        find_turns(x, y, board->get_board());
    }

private:
    // Находит все возможные ходы для фигур указанного цвета (color: 0 - белые, 1 - черные)
    // на указанной доске mtx и сохраняет их в вектор turns.
    // Если есть возможность взятия (побития), то сохраняются только ходы со взятием,
    // так как по правилам игры взятие обязательно.
    void find_turns(const bool color, const vector<vector<POS_T>> &mtx)
    {
        vector<move_pos> res_turns;
        bool have_beats_before = false;
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                if (mtx[i][j] && mtx[i][j] % 2 != color)
                {
                    find_turns(i, j, mtx);
                    if (have_beats && !have_beats_before)
                    {
                        have_beats_before = true;
                        res_turns.clear();
                    }
                    if ((have_beats_before && have_beats) || !have_beats_before)
                    {
                        res_turns.insert(res_turns.end(), turns.begin(), turns.end());
                    }
                }
            }
        }
        turns = res_turns;
        shuffle(turns.begin(), turns.end(), rand_eng);  // Перемешиваем ходы для разнообразия игры
        have_beats = have_beats_before;
    }
    // Находит все возможные ходы для фигуры на позиции (x, y) на указанной доске mtx
    // и сохраняет их в вектор turns. Сначала проверяются возможные взятия (побития),
    // и если они есть, то обычные ходы не рассматриваются, так как взятие обязательно.
    // Учитывает различные типы фигур (шашки и дамки) и их особенности ходов.
    void find_turns(const POS_T x, const POS_T y, const vector<vector<POS_T>> &mtx)
    {
        turns.clear();
        have_beats = false;
        POS_T type = mtx[x][y];
        // check beats
        switch (type)
        {
        case 1:  // Белая шашка
        case 2:  // Черная шашка
            // check pieces
            for (POS_T i = x - 2; i <= x + 2; i += 4)
            {
                for (POS_T j = y - 2; j <= y + 2; j += 4)
                {
                    if (i < 0 || i > 7 || j < 0 || j > 7)
                        continue;
                    POS_T xb = (x + i) / 2, yb = (y + j) / 2;
                    if (mtx[i][j] || !mtx[xb][yb] || mtx[xb][yb] % 2 == type % 2)
                        continue;
                    turns.emplace_back(x, y, i, j, xb, yb);
                }
            }
            break;
        default:  // Дамка (белая - 3, черная - 4)
            // check queens
            for (POS_T i = -1; i <= 1; i += 2)
            {
                for (POS_T j = -1; j <= 1; j += 2)
                {
                    POS_T xb = -1, yb = -1;
                    for (POS_T i2 = x + i, j2 = y + j; i2 != 8 && j2 != 8 && i2 != -1 && j2 != -1; i2 += i, j2 += j)
                    {
                        if (mtx[i2][j2])
                        {
                            if (mtx[i2][j2] % 2 == type % 2 || (mtx[i2][j2] % 2 != type % 2 && xb != -1))
                            {
                                break;
                            }
                            xb = i2;
                            yb = j2;
                        }
                        if (xb != -1 && xb != i2)
                        {
                            turns.emplace_back(x, y, i2, j2, xb, yb);
                        }
                    }
                }
            }
            break;
        }
        // check other turns
        if (!turns.empty())
        {
            have_beats = true;
            return;
        }
        switch (type)
        {
        case 1:  // Белая шашка
        case 2:  // Черная шашка
            // check pieces
            {
                POS_T i = ((type % 2) ? x - 1 : x + 1);  // Белые ходят вверх, черные - вниз
                for (POS_T j = y - 1; j <= y + 1; j += 2)
                {
                    if (i < 0 || i > 7 || j < 0 || j > 7 || mtx[i][j])
                        continue;
                    turns.emplace_back(x, y, i, j);
                }
                break;
            }
        default:  // Дамка (белая - 3, черная - 4)
            // check queens
            for (POS_T i = -1; i <= 1; i += 2)
            {
                for (POS_T j = -1; j <= 1; j += 2)
                {
                    for (POS_T i2 = x + i, j2 = y + j; i2 != 8 && j2 != 8 && i2 != -1 && j2 != -1; i2 += i, j2 += j)
                    {
                        if (mtx[i2][j2])
                            break;
                        turns.emplace_back(x, y, i2, j2);
                    }
                }
            }
            break;
        }
    }

  public:
    vector<move_pos> turns;
    bool have_beats;
    int Max_depth;

  private:
    default_random_engine rand_eng;
    string scoring_mode;
    string optimization;
    vector<move_pos> next_move;
    vector<int> next_best_state;
    Board *board;
    Config *config;
};
