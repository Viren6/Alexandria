#include "bench.h"
#include "uci.h"
#include "makemove.h"
#include "misc.h"
#include "types.h"
#include "perft.h"
#include "search.h"
#include "time_manager.h"
#include "ttable.h"
#include "init.h"
#include "io.h"
#include "threads.h"
#include "board.h"
#include "movegen.h"
#include <iostream>
#include "tune.h"
#include "eval.h"

// convert a move to coordinate notation to internal notation
int ParseMove(const std::string& move_string, S_Board* pos) {
    // create move list instance
    S_MOVELIST moveList[1];

    // generate moves
    GenerateMoves(moveList, pos);

    // parse source square
    const int sourceSquare = (move_string[0] - 'a') + (8 - (move_string[1] - '0')) * 8;

    // parse target square
    const int targetSquare = (move_string[2] - 'a') + (8 - (move_string[3] - '0')) * 8;

    // loop over the moves within a move list
    for (int move_count = 0; move_count < moveList->count; move_count++) {
        // init move
        const int move = moveList->moves[move_count].move;
        // make sure source & target squares are available within the generated move
        if (sourceSquare == From(move) &&
            targetSquare == To(move)) {
            // promoted piece is available
            if (isPromo(move)) {
                switch (getPromotedPiecetype(move)) {
                case QUEEN:
                    if (move_string[4] == 'q')
                        return move;
                    break;
                case ROOK:
                    if (move_string[4] == 'r')
                    return move;
                    break;
                case BISHOP:
                    if (move_string[4] == 'b')
                    return move;
                    break;
                case KNIGHT:
                    if (move_string[4] == 'n')
                    return move;
                    break;
                }
                // continue the loop on possible wrong promotions (e.g. "e7e8f")
                continue;
            }
            // return legal move
            return move;
        }
    }
    std::cout << "Illegal move parsed: " << move_string;
    // return illegal move
    return NOMOVE;
}

// parse UCI "position" command
void ParsePosition(const std::string& command, S_Board* pos) {
    // parse UCI "startpos" command
    if (command.find("startpos") != std::string::npos) {
        // init chess board with start position
        ParseFen(start_position, pos);
    }

    // parse UCI "fen" command
    else {
        // if a "fen" command is available within command string
        if (command.find("fen") != std::string::npos) {
            // init chess board with position from FEN string
            ParseFen(command.substr(command.find("fen") + 4, std::string::npos), pos);
        }
        else {
            // init chess board with start position
            ParseFen(start_position, pos);
        }
    }

    // if there are moves to be played in the fen play them
    if (command.find("moves") != std::string::npos) {
        int string_start = command.find("moves") + 6;
        // Avoid looking for a moves that doesn't exist in the case of "position startpos moves <blank>" (Needed for Scid support)
        if (!(string_start > command.length())) {
            std::string moves_substr = command.substr(string_start, std::string::npos);
            parse_moves(moves_substr, pos);
        }
    }

    // Update accumulator state to reflect the new position
    Accumulate(pos->accumStack[0], pos);
    pos->accumStackHead = 1;
}

// parse UCI "go" command, returns true if we have to search afterwards and false otherwise
bool ParseGo(const std::string& line, S_SearchINFO* info, S_Board* pos) {
    ResetInfo(info);
    int depth = -1, movetime = -1;
    int movestogo;
    int time = -1, inc = 0;

    std::vector<std::string> tokens = split_command(line);

    // loop over all the tokens and parse the commands
    for (size_t i = 1; i < tokens.size(); i++) {
        if (tokens.at(1) == "infinite") {
            ;
        }

        if (tokens.at(1) == "perft") {
            int perft_depth = std::stoi(tokens[2]);
            PerftTest(perft_depth, pos);
            return false;
        }

        if (tokens.at(i) == "binc" && pos->side == BLACK) {
            inc = std::stoi(tokens[i + 1]);
        }

        if (tokens.at(i) == "winc" && pos->side == WHITE) {
            inc = std::stoi(tokens[i + 1]);
        }

        if (tokens.at(i) == "wtime" && pos->side == WHITE) {
            time = std::stoi(tokens[i + 1]);
            info->timeset = true;
        }
        if (tokens.at(i) == "btime" && pos->side == BLACK) {
            time = std::stoi(tokens[i + 1]);
            info->timeset = true;
        }

        if (tokens.at(i) == "movestogo") {
            movestogo = std::stoi(tokens[i + 1]);
            if (movestogo > 0)
                info->movestogo = movestogo;
        }

        if (tokens.at(i) == "movetime") {
            movetime = std::stoi(tokens[i + 1]);
            time = movetime;
            info->movetimeset = true;
        }

        if (tokens.at(i) == "depth") {
            depth = std::stoi(tokens[i + 1]);
        }

        if (tokens.at(i) == "nodes") {
            info->nodeset = true;
            info->nodeslimit = std::stoi(tokens[i + 1]);
        }
    }

    info->starttime = GetTimeMs();
    info->depth = depth;
    // calculate time allocation for the move
    Optimum(info, time, inc);

    if (depth == -1) {
        info->depth = MAXDEPTH;
    }
    std::cout << "info ";
    std::cout << "time: " << time << " ";
    std::cout << "start: " << info->starttime << " ";
    std::cout << "stopOpt: " << info->stoptimeOpt << " ";
    std::cout << "stopMax: " << info->stoptimeMax << " ";
    std::cout << "depth: " << info->depth << " ";
    std::cout << "timeset: " << info->timeset << " ";
    std::cout << "nodeset: " << info->nodeset << "\n";
    return true;
}

// main UCI loop
void UciLoop(int argc, char** argv) {
    if (argv[1] && strncmp(argv[1], "bench", 5) == 0) {
        int benchDepth = 14;
        // If there's an additional input try to parse it as a bench depth
        if (argc == 3) {
            if (std::stoi(argv[2]) > 0) {
                benchDepth = std::stoi(argv[2]);
            }
            else {
                std::cout << "Invalid bench depth";
                return;
            }
        }
        tryhardmode = true;
        StartBench(benchDepth);
        return;
    }

    bool parsed_position = false;
    S_UciOptions uci_options[1];
    S_ThreadData* td(new S_ThreadData());
    std::thread main_thread;
    state threads_state = Idle;
    // print engine info
    std::cout << NAME << "\n";

#ifdef TUNE
    for (tunable_param param : tunableParams)
        std::cout << param << "\n";
#endif

    // main loop
    while (true) {
        // define user / GUI input buffer
        std::string input;

        // get user / GUI input
        if (!std::getline(std::cin, input)) {
            // continue the loop
            break;
        }

        // make sure input is available
        if (!input.length()) {
            // continue the loop
            continue;
        }

        // Split the string into tokens to make it easier to work with
        std::vector<std::string> tokens = split_command(input);

        // parse UCI "position" command
        if (tokens[0] == "position") {
            // call parse position function
            ParsePosition(input, &td->pos);
            parsed_position = true;
        }

        // parse UCI "go" command
        else if (tokens[0] == "go") {
            StopHelperThreads();
            // Join previous search thread if it exists
            if (main_thread.joinable())
                main_thread.join();

            if (!parsed_position) { // call parse position function
                ParsePosition("position startpos", &td->pos);
            }
            // call parse go function
            bool search = ParseGo(input, &td->info, &td->pos);
            // Start search in a separate thread
            if (search) {
                threads_state = Search;
                main_thread = std::thread(RootSearch, td->info.depth, td, uci_options);
            }
        }

        else if (tokens[0] == "setoption") {
            // check tokens for size to see if we have a value
            if (tokens.size() < 5) {
                std::cout << "Invalid setoption format" << "\n";
                continue;
            }
            if (tokens.at(2) == "Hash") {
                uci_options->Hash = std::stoi(tokens.at(4));
                std::cout << "Set Hash to " << uci_options->Hash << " MB\n";
                InitHashTable(HashTable, uci_options->Hash);
            }
            else if (tokens.at(2) == "Threads") {
                uci_options->Threads = std::stoi(tokens.at(4));
                std::cout << "Set Threads to " << uci_options->Threads << "\n";
            }
#ifdef TUNE
            else {
                updateTuneVariable(tokens.at(2), std::stoi(tokens.at(4)));
        }
#else
            else std::cout << "Unknown command: " << input << std::endl;
#endif
        }

        // parse UCI "isready" command
        else if (input == "isready") {
            std::cout << "readyok\n";
            continue;
        }

        // parse UCI "ucinewgame" command
        else if (input == "ucinewgame") {
            InitNewGame(td);
        }
        // parse UCI "stop" command
        else if (input == "stop") {
            if (threads_state == Search) {
                // Stop helper threads
                StopHelperThreads();
                // stop main thread search
                td->info.stopped = true;
            } 
            threads_state = Idle;
        }

        // parse UCI "quit" command
        else if (input == "quit" || input == "exit") {
            if (threads_state == Search) {
                // Stop helper threads
                StopHelperThreads();
                // stop main thread search
                td->info.stopped = true;
            }
            threads_state = Idle;
            // Join previous search thread if it exists
            if (main_thread.joinable())
                main_thread.join();
            // free thread data
            delete td;
            // quit from the chess engine program execution
            break;
        }

        // parse UCI "uci" command
        else if (input == "uci") {
            // print engine info
            std::cout << "id name " << NAME << "\n";
            std::cout << "id author PGG and Contributors\n";
            std::cout << "option name Hash type spin default 16 min 1 max 262144 \n";
            std::cout << "option name Threads type spin default 1 min 1 max 256 \n";
#ifdef TUNE
            for (tunable_param param : tunableParams) {
                std::cout << "option name " + param.name;
                std::cout << " type spin default ";
                std::cout << param.currValue;
                std::cout << " min ";
                std::cout << param.minValue;
                std::cout << " max ";
                std::cout << param.maxValue << "\n";
            }
#endif
            std::cout << "uciok\n";
            // Set uci compatible output mode
            print_uci = true;
        }

        // print board
        else if (input == "d") {
            PrintBoard(&td->pos);
        }

        // spsa info dump
        else if (input == "tune") {
            for (tunable_param param : tunableParams) {
                std::cout << param << "\n";
            }
        }

        else if (input == "eval") {// call parse position function
            if (!parsed_position) {
                ParsePosition("position startpos", &td->pos);
            }
            // print position eval
            bool stm = td->pos.side == WHITE;
            std::cout << "Raw eval: " << nnue.output(td->pos.AccumulatorTop(), stm) << std::endl;
            std::cout << "Scaled eval: " << EvalPosition(&td->pos) << std::endl;
        }

        else if (input == "bench") {
            tryhardmode = true;
            StartBench();
            tryhardmode = false;
        }

        else if (input == "see") {
            // create move list instance
            S_MOVELIST move_list[1];

            // generate moves
            GenerateMoves(move_list, &td->pos);
            printf("SEE thresholds\n");
            for (int i = 0; i < move_list->count; i++) {
                int move = move_list->moves[i].move;
                for (int j = 1200; j > -1200; j--) {
                    if (SEE(&td->pos, move, j)) {
                        printf(" move number %d  %s SEE result: %d \n", i + 1, FormatMove(move), j);
                        break;
                    }
                }
            }
        }
        else std::cout << "Unknown command: " << input << std::endl;
    }
}
