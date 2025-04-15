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

    // Ôóíêöèÿ play() çàïóñêàåò è óïðàâëÿåò èãðîâûì ïðîöåññîì øàøåêпа
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
        while (++turn_num < Max_turns)  // Öèêë èãðû, ïðîäîëæàåòñÿ äî äîñòèæåíèÿ ìàêñèìàëüíîãî ÷èñëà õîäîâ
        {
            beat_series = 0;  // Ñáðîñ ñ÷åò÷èêà ñåðèè âçÿòèé
            logic.find_turns(turn_num % 2);  // Ïîèñê âîçìîæíûõ õîäîâ äëÿ òåêóùåãî èãðîêà (0 - áåëûå, 1 - ÷åðíûå)
            if (logic.turns.empty())  // Ïðîâåðêà íà îêîí÷àíèå èãðû - åñëè õîäîâ íåò, èãðà çàâåðøàåòñÿ
                break;
            // Îïðåäåëåíèå óðîâíÿ ñëîæíîñòè áîòà äëÿ òåêóùåãî èãðîêà
            logic.Max_depth = config("Bot", string((turn_num % 2) ? "Black" : "White") + string("BotLevel"));
            // Ïðîâåðêà, êòî ñåé÷àñ õîäèò - èãðîê èëè áîò
            if (!config("Bot", string("Is") + string((turn_num % 2) ? "Black" : "White") + string("Bot")))
            {
                // Îáðàáîòêà õîäà èãðîêà
                auto resp = player_turn(turn_num % 2);
                if (resp == Response::QUIT)  // Åñëè èãðîê ðåøèë âûéòè
                {
                    is_quit = true;
                    break;
                }
                else if (resp == Response::REPLAY)  // Åñëè èãðîê ðåøèë íà÷àòü çàíîâî
                {
                    is_replay = true;
                    break;
                }
                else if (resp == Response::BACK)  // Åñëè èãðîê ðåøèë îòìåíèòü õîä
                {
                    // Ëîãèêà îòìåíû õîäîâ ñ ó÷åòîì òèïà ïðîòèâíèêà (áîò/èãðîê)
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
                bot_turn(turn_num % 2);  // Îáðàáîòêà õîäà áîòà
        }
        // Çàïèñü âðåìåíè èãðû â ëîã
        auto end = chrono::steady_clock::now();
        ofstream fout(project_path + "log.txt", ios_base::app);
        fout << "Game time: " << (int)chrono::duration<double, milli>(end - start).count() << " millisec\n";
        fout.close();

        if (is_replay)  // Åñëè çàïðîøåíà íîâàÿ èãðà, ðåêóðñèâíî âûçûâàåì play()
            return play();
        if (is_quit)  // Åñëè èãðà çàâåðøåíà âûõîäîì, âîçâðàùàåì 0
            return 0;
        // Îïðåäåëåíèå ðåçóëüòàòà èãðû
        int res = 2;  // Ïî óìîë÷àíèþ - íè÷üÿ
        if (turn_num == Max_turns)
        {
            res = 0;  // Íè÷üÿ ïî âðåìåíè
        }
        else if (turn_num % 2)
        {
            res = 1;  // Ïîáåäà áåëûõ
        }
        // Èíà÷å ïîáåäà ÷åðíûõ (res = 2)

        board.show_final(res);  // Îòîáðàæåíèå ôèíàëüíîãî ðåçóëüòàòà

        // Îæèäàíèå äåéñòâèÿ èãðîêà ïîñëå îêîí÷àíèÿ èãðû
        auto resp = hand.wait();
        if (resp == Response::REPLAY)
        {
            is_replay = true;
            return play();  // Íà÷àòü íîâóþ èãðó
        }
        return res;  // Âåðíóòü ðåçóëüòàò èãðû
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
    // Ôóíêöèÿ player_turn() îáðàáàòûâàåò õîä èãðîêà óêàçàííîãî öâåòà (color: 0 - áåëûå, 1 - ÷åðíûå)
    // Âîçâðàùàåò Response ñ ðåçóëüòàòîì äåéñòâèÿ èãðîêà
    Response player_turn(const bool color)
    {
        // Ñîçäàåì ñïèñîê êëåòîê, ñîäåðæàùèõ ôèãóðû, êîòîðûìè ìîæíî õîäèòü
        vector<pair<POS_T, POS_T>> cells;
        for (auto turn : logic.turns)
        {
            cells.emplace_back(turn.x, turn.y);
        }
        board.highlight_cells(cells);  // Ïîäñâå÷èâàåì ýòè êëåòêè íà äîñêå
        move_pos pos = {-1, -1, -1, -1};  // Èíèöèàëèçàöèÿ ñòðóêòóðû õîäà
        POS_T x = -1, y = -1;  // Êîîðäèíàòû âûáðàííîé ôèãóðû
        // Öèêë äëÿ âûáîðà ôèãóðû è ïîëó÷åíèÿ ïåðâîãî õîäà
        while (true)
        {
            auto resp = hand.get_cell();  // Îæèäàíèå âûáîðà êëåòêè èãðîêîì
            if (get<0>(resp) != Response::CELL)  // Åñëè ïîëó÷åí íå âûáîð êëåòêè, à äðóãîå äåéñòâèå
                return get<0>(resp);  // Âîçâðàùàåì ýòî äåéñòâèå (QUIT, REPLAY, BACK)
            pair<POS_T, POS_T> cell{get<1>(resp), get<2>(resp)};  // Ïîëó÷àåì êîîðäèíàòû âûáðàííîé êëåòêè

            bool is_correct = false;  // Ôëàã êîððåêòíîñòè âûáðàííîé êëåòêè
            // Ïðîâåðÿåì, ÿâëÿåòñÿ ëè âûáîð êîððåêòíûì (íà÷àëî õîäà èëè êîíåö õîäà)
            for (auto turn : logic.turns)
            {
                if (turn.x == cell.first && turn.y == cell.second)  // Åñëè âûáðàíà êëåòêà ñ ôèãóðîé, êîòîðîé ìîæíî õîäèòü
                {
                    is_correct = true;
                    break;
                }
                if (turn == move_pos{x, y, cell.first, cell.second})  // Åñëè óæå âûáðàíà ôèãóðà è ýòî âûáîð êóäà õîäèòü
                {
                    pos = turn;  // Çàïîìèíàåì ïîëíîñòüþ õîä
                    break;
                }
            }
            if (pos.x != -1)  // Åñëè õîä ïîëíîñòüþ îïðåäåëåí
                break;
            if (!is_correct)  // Åñëè âûáðàíà íåêîððåêòíàÿ êëåòêà
            {
                if (x != -1)  // Åñëè ôèãóðà óæå áûëà âûáðàíà, ñáðàñûâàåì âûáîð
                {
                    board.clear_active();  // Óáèðàåì àêòèâíóþ ôèãóðó
                    board.clear_highlight();  // Óáèðàåì ïîäñâåòêó âîçìîæíûõ õîäîâ
                    board.highlight_cells(cells);  // Ñíîâà ïîäñâå÷èâàåì ôèãóðû, êîòîðûìè ìîæíî õîäèòü
                }
                x = -1;
                y = -1;
                continue;
            }
            // Çàïîìèíàåì êîîðäèíàòû âûáðàííîé ôèãóðû
            x = cell.first;
            y = cell.second;
            board.clear_highlight();  // Óáèðàåì ïðåäûäóùóþ ïîäñâåòêó
            board.set_active(x, y);  // Ïîìå÷àåì âûáðàííóþ ôèãóðó êàê àêòèâíóþ
            // Ïîäñâå÷èâàåì êëåòêè, êóäà ìîæíî ïîéòè âûáðàííîé ôèãóðîé
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
        board.clear_highlight();  // Óáèðàåì ïîäñâåòêó
        board.clear_active();  // Óáèðàåì àêòèâíóþ ôèãóðó
        board.move_piece(pos, pos.xb != -1);  // Âûïîëíÿåì õîä (ñ ïîáèòèåì èëè áåç)
        if (pos.xb == -1)  // Åñëè íå áûëî âçÿòèÿ ôèãóðû
            return Response::OK;  // Õîä çàâåðøåí

        // Îáðàáîòêà ñåðèè âçÿòèé (åñëè åñòü âîçìîæíîñòü ïðîäîëæèòü áèòü)
        beat_series = 1;  // Ñ÷åò÷èê ñåðèè âçÿòèé
        while (true)
        {
            logic.find_turns(pos.x2, pos.y2);  // Èùåì âîçìîæíûå õîäû ñ íîâîé ïîçèöèè
            if (!logic.have_beats)  // Åñëè íåò âîçìîæíîñòè âçÿòèÿ, çàâåðøàåì ñåðèþ
                break;

            // Ïîäñâå÷èâàåì êëåòêè, êóäà ìîæíî ñäåëàòü ñëåäóþùèé õîä â ñåðèè âçÿòèé
            vector<pair<POS_T, POS_T>> cells;
            for (auto turn : logic.turns)
            {
                cells.emplace_back(turn.x2, turn.y2);
            }
            board.highlight_cells(cells);
            board.set_active(pos.x2, pos.y2);  // Ïîìå÷àåì òåêóùóþ ïîçèöèþ ôèãóðû êàê àêòèâíóþ
            // Öèêë äëÿ âûáîðà ñëåäóþùåãî õîäà â ñåðèè âçÿòèé
            while (true)
            {
                auto resp = hand.get_cell();  // Îæèäàíèå âûáîðà êëåòêè èãðîêîì
                if (get<0>(resp) != Response::CELL)  // Åñëè ïîëó÷åí íå âûáîð êëåòêè, à äðóãîå äåéñòâèå
                    return get<0>(resp);  // Âîçâðàùàåì ýòî äåéñòâèå (QUIT, REPLAY, BACK)
                pair<POS_T, POS_T> cell{get<1>(resp), get<2>(resp)};  // Ïîëó÷àåì êîîðäèíàòû âûáðàííîé êëåòêè

                bool is_correct = false;  // Ôëàã êîððåêòíîñòè âûáðàííîé êëåòêè
                // Ïðîâåðÿåì, ÿâëÿåòñÿ ëè âûáîð êîððåêòíûì (êóäà ìîæíî ñõîäèòü)
                for (auto turn : logic.turns)
                {
                    if (turn.x2 == cell.first && turn.y2 == cell.second)  // Åñëè âûáðàíà êëåòêà, êóäà ìîæíî ñõîäèòü
                    {
                        is_correct = true;
                        pos = turn;  // Çàïîìèíàåì õîä
                        break;
                    }
                }
                if (!is_correct)  // Åñëè âûáðàíà íåêîððåêòíàÿ êëåòêà
                    continue;  // Æäåì ïðàâèëüíîãî âûáîðà

                board.clear_highlight();  // Óáèðàåì ïîäñâåòêó
                board.clear_active();  // Óáèðàåì àêòèâíóþ ôèãóðó
                beat_series += 1;  // Óâåëè÷èâàåì ñ÷åò÷èê ñåðèè âçÿòèé
                board.move_piece(pos, beat_series);  // Âûïîëíÿåì õîä
                break;
            }
        }

        return Response::OK;  // Õîä óñïåøíî çàâåðøåí
    }

  private:
    Config config;
    Board board;
    Hand hand;
    Logic logic;
    int beat_series;
    bool is_replay = false;
};
