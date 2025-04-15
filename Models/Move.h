#pragma once
#include <stdlib.h>

// Тип данных для координат на доске (8-битное целое со знаком)
typedef int8_t POS_T;

// Структура move_pos описывает один ход в игре шашки
// Содержит координаты начальной клетки, конечной клетки и побитой шашки (если есть)
struct move_pos
{
    POS_T x, y;             // Координаты начальной позиции (откуда ходим)
    POS_T x2, y2;           // Координаты конечной позиции (куда ходим)
    POS_T xb = -1, yb = -1; // Координаты побитой шашки (если есть, иначе -1)

    // Конструктор для обычного хода без взятия
    move_pos(const POS_T x, const POS_T y, const POS_T x2, const POS_T y2) : x(x), y(y), x2(x2), y2(y2)
    {
    }
    // Конструктор для хода со взятием (указываются координаты побитой шашки)
    move_pos(const POS_T x, const POS_T y, const POS_T x2, const POS_T y2, const POS_T xb, const POS_T yb)
        : x(x), y(y), x2(x2), y2(y2), xb(xb), yb(yb)
    {
    }

    // Оператор сравнения для ходов (сравниваются только начальные и конечные позиции)
    bool operator==(const move_pos &other) const
    {
        return (x == other.x && y == other.y && x2 == other.x2 && y2 == other.y2);
    }
    // Оператор неравенства для ходов
    bool operator!=(const move_pos &other) const
    {
        return !(*this == other);
    }
};
