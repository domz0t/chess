#include <iostream>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <vector>
#include <fstream>
#include <regex>
#include <limits>

using namespace std;

struct termios saved_attr;

void reset_input_mode() {
    tcsetattr(STDIN_FILENO, TCSANOW, &saved_attr);
}

void set_input_mode() {
    struct termios tattr;
    if(!isatty(STDIN_FILENO)) {
        fprintf(stderr, "Not a terminal!");
        exit(EXIT_FAILURE);
    }
    tcgetattr(STDIN_FILENO, &saved_attr);
    atexit(reset_input_mode);

    tcgetattr(STDIN_FILENO, &tattr);
    tattr.c_lflag &= ~(ICANON|ECHO);
    tattr.c_cc[VMIN] = 1;
    tattr.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &tattr);
}
 
void non_block_read() {
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
}

void block_read() {
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | F_RDLCK);
}

int range(int x, int min, int max)
{
    if(x < min)
        return min;
    else if(x > max)
        return max;
    return x;
}

int is_in(char letter, char* alp)
{
    for(int i = 0; i < sizeof(alp); i++)
        if(letter == alp[i]) return i;
    return -1;
}

enum Color {WHITE, BLACK, UNCOLORED};
enum Obj {King, Queen, Rook, Bishop, Knight, Pawn, Square, Unknown};
enum Direction {Forward, Backward, NOWHERE};
enum Regime {Classic, View, Menu};
enum State {LongCastling, ShortCastling, EnPassant, Nothing};

Color reverse_color(Color color) { return (color == WHITE ? BLACK : WHITE); }

class Highlight
{
    int x, y;
    char* cmd;

    public:
    Highlight(int _x, int _y, char* _cmd) : x(_x), y(_y), cmd(_cmd) {}
    int get_x() const {return x; }
    void set_x(int _x) { x = _x; }
    int get_y() const {return y; }
    void set_y(int _y) { y = _y; }
    char* get_cmd() const {return cmd; }
};

class Board;

class Object
{
    Color clr;
    char* img;
    int x, y;
    int extra;
    Obj type;
    int links;

    public:
    Object(int _x, int _y, Color _clr = WHITE) : clr(_clr), x(_x), y(_y), extra(0), links(0) {}
    Color   get_color()       const { return clr; }
    void    set_color(Color _clr)   { clr = _clr; }
    char*   get_img()         const { return img; }
    void    set_img(char* _img)     { img = _img; }
    int     get_x()           const { return x; }
    void    set_x(int _x)           { x = _x; }
    int     get_y()           const { return y; }
    void    set_y(int _y)           { y = _y; }
    int     get_extra()       const { return extra; }
    void    inc_extra()             { extra++; }
    void    dec_extra()             { extra--; }
    void    set_extra(int _extra)   { extra = _extra; }
    Obj     get_type()        const { return type; }
    void    set_type(Obj _type)     { type = _type; }
    int     get_links()       const { return links; }
    void    inc_links()             { links++; }
    void    dec_links()             { links--; }


    virtual bool is_legal(Object* obj, Board* board) { return false; };
    virtual ~Object() {}
};

class Square : public Object
{
    public:
    Square(int _x, int _y, Color _clr) : Object(_x, _y, _clr) {
        if(this -> get_color() == WHITE)
            this -> set_img((char*)"\u25A1");
        else
            this -> set_img((char*)"\u25A0");
        this->set_type(Obj::Square);
    }
    virtual bool is_legal(Object* obj, Board* board) 
    {
        return false;
    }

};

class Turn
{
    Object* obj_from;
    Object* obj_to;
    Object* obj_replace;
    int extra_index;
    double ai_evaluation;

    public:
    Turn(Object* _from, Object* _to, int _castling_index = -1) : obj_from(_from), obj_to(_to), extra_index(_castling_index)
    {
        int x = obj_from->get_x();
        int y = obj_from->get_y();
        obj_replace = new class Square(x, y, (((x + y) % 2 == 1) ? WHITE : BLACK));
    }
    Object* get_from() const { return obj_from; }
    Object* get_to() const { return obj_to; }
    Object* get_replace() const { return obj_replace; }
    void set_from(Object* _from)  { obj_from = _from; }
    void set_to(Object* _to) { obj_to = _to; }
    void set_replace(Object* _replace) { obj_replace = _replace; }
    int get_extra_index()  const { return extra_index; }
    void set_extra_index(int _extra_index)  { extra_index = _extra_index; }
    double get_ai_evaluation()  const { return ai_evaluation; }
    void set_ai_evaluation(double _ai_evaluation)  { ai_evaluation = _ai_evaluation; }
};

class AI
{
    int standard_pawn_reward[8] = {0, 0, 0, 0, 10, 20, 30, 0};
    int passed_pawn_reward[8] = {0, 50, 50, 50, 70, 90, 110, 0};

    public:
    double check_mobility(Object* obj, Board* board);
    bool check_passed_pawn(Object* obj, Board* board);
    double static_analyze(Board* board);
    static bool compareTurnsForWhite(Turn* turn_1, Turn* turn_2)
    {
        return (turn_1->get_ai_evaluation() > turn_2->get_ai_evaluation());
    }
    static bool compareTurnsForBlack(Turn* turn_1, Turn* turn_2)
    {
        return (turn_1->get_ai_evaluation() < turn_2->get_ai_evaluation());
    }
    double evaluate_best_answer(Board* board, Color turn_color, int depth);
    Turn* analyze(Board* board, Color turn_color);
};

class Board
{
    int width = 8;
    int height = 8;

    char* high_alphabet = (char*)"ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    char* low_alphabet = (char*)"abcdefghijklmnopqrstuvwxyz";
    Object** board;
    AI ai;
    vector<Highlight*> hl_v;
    Object* hit_field = NULL;
    int inside_counter = 0;

    Object* white_king;
    bool white_castling;
    Object* black_king;
    bool black_castling;
    bool double_check = false;
    bool board_flipped = false;
    string game_info[10];
    bool skip = false;
    vector<string> notation_turns;
    int turn = -1;
    vector<Turn*> turns;
    vector<Turn*> extra_turns;
    int free_extra_index;
    Regime regime;
    State cur_state;
    bool AI_state;

    public:
    Board()
    {
        board = new Object*[width * height];
        for(int i = height - 1; i >= 0; i--)
            for(int j = 0; j < width; j++)
                if((i + j) % 2 == 1)
                    board[i * width + j] = new class Square(j, i, WHITE);
                else    
                    board[i * width + j] = new class Square(j, i, BLACK);
        free_extra_index = 0;
        AI_state = false;
    }
    friend class AI;
    int get_width() const { return width; }
    int get_height() const { return height; }
    void add(Object* obj)
    {
        delete board[obj->get_y() * width + obj->get_x()];
        board[obj->get_y() * width + obj->get_x()] = obj;
        if(obj->get_type() == King)
        {
            if(obj->get_color() == WHITE)
                white_king = obj;
            else    
                black_king = obj;
        }
    }
    void add_wd(Object* obj)
    {
        board[obj->get_y() * width + obj->get_x()] = obj;
        if(obj->get_type() == King)
        {
            if(obj->get_color() == WHITE)
                white_king = obj;
            else    
                black_king = obj;
        }
    }
    Object* get(int x, int y)
    {
        if((x < 0) || (x >= width) || (y < 0) || (y >= height)) return NULL;
        return board[y * width + x];
    }
    void print_board()
    {
        if(board_flipped)
        {
            print_flipped_board();
            return;
        }
        system("clear");
        for(int i = height - 1; i >= 0; i--)
        {
            cout << i + 1 << " ";
            for(int j = 0; j < width; j++)
            {
                for(Highlight* k : hl_v)
                    if(k -> get_x() == j && k -> get_y() == i)
                        cout << k -> get_cmd();
                cout << board[i * width + j]->get_img() << " ";
                cout << "\x1b[0m";
            }
            cout << "\t" <<  game_info[height - i - 1];
        }
        cout << "  ";
        for(int k = 0; k < width; k++)
            cout << high_alphabet[k] << " ";
        cout << "\t" <<  game_info[8];
    }
    void print_flipped_board()
    {
        system("clear");
        for(int i = 0; i < height; i++)
        {
            cout << i + 1 << " ";
            for(int j = width - 1; j >= 0; j--)
            {
                for(Highlight* k : hl_v)
                    if(k -> get_x() == j && k -> get_y() == i)
                        cout << k -> get_cmd();
                cout << board[i * width + j]->get_img() << " ";
                cout << "\x1b[0m";
            }
            cout << "\t" <<  game_info[i];
        }
        cout << "  ";
        for(int k = 0; k < width; k++)
            cout << high_alphabet[k] << " ";
        cout << "\t" <<  game_info[8];
    }
    void add_highliter(Highlight* hl) { hl_v.push_back(hl); }
    void pop_last_highliter() { hl_v.pop_back(); }
    Object* get_hit_field() const { return hit_field; }
    void set_hit_field(Object* _hit_field) { hit_field = _hit_field; }
    int get_turn() const {return turn; }
    int  get_inside_counter() const { return inside_counter; }
    void set_inside_counter(int _inside_counter) { inside_counter = _inside_counter; }
    void dec_inside_counter() { inside_counter--; }
    void set_cur_state(State _cur_state) { cur_state = _cur_state; }
    void clear()
    {
        Object* obj;
        for(int y = 0; y < height; y++)
        {
            for(int x = 0; x < width; x++)
            {
                obj = this->get(x, y);
                if(obj->get_type() != Square)
                    this->add(new class Square(x, y, ((x + y) % 2 == 1) ? WHITE : BLACK));
                
            }
        }
    }
    void set_start_position();
    bool get_board_flipped() { return board_flipped; }
    Object* is_hitted(Object* obj, Color color = UNCOLORED, Obj type = Unknown, int x_hint = -1, int y_hint = -1)
    {
        int threats_count = 0;
        Object* threat = NULL; 
        for(int y = 0; y < height; y++)
        {
            for(int x = 0; x < width; x++)
            {
                if((color != UNCOLORED) && (this->get(x, y)->get_color() != color)) continue;
                if((type != Unknown) && (this->get(x, y)->get_type() != type)) continue;
                if((x_hint != -1) && (x != x_hint)) continue;
                if((y_hint != -1) && (y != y_hint)) continue;
                if(this->get(x, y)->is_legal(obj, this))
                {
                    threat = this->get(x, y);
                    threats_count++;
                }
            }
        }
        if(threats_count > 1) double_check = true;
        return threat;
    }
    Object* check_king_dependency(Object* figure, Object* next_stop)
    {
        Object* king = (figure->get_color() == WHITE ? this->get_white_king() : this->get_black_king());
        int x = figure->get_x();
        int y = figure->get_y();
        int new_x = next_stop->get_x();
        int new_y = next_stop->get_y();

        Object* temp = this->get(new_x, new_y);
        figure->set_x(new_x);
        figure->set_y(new_y);
        this->add_wd(figure);
        this->add_wd(new class Square(x, y, (x + y) % 2 == 1 ? WHITE : BLACK)); 

        Object* is_hitted = this->is_hitted(king);
        double_check = false;

        figure->set_x(x);
        figure->set_y(y);
        this->add(figure);
        this->add_wd(temp);

        return is_hitted;
    }
    bool check_king_overlap(Object* king, Object* checker)
    {
        int x = checker->get_x();
        int y = checker->get_y();
        int new_x = king->get_x();
        int new_y = king->get_y();
        Color color = king->get_color();
        Object* possible_blocker;
        if((checker->get_type() == Knight) || (checker->get_type() == Pawn))
        {
            possible_blocker = this->is_hitted(checker, color);
            if(
                possible_blocker != NULL
                && (this->check_king_dependency(possible_blocker, this->get(x, y)) == NULL)
            )
                return true;
        }
        if((checker->get_type() == Bishop) || (checker->get_type() == Queen))
        {
            if(abs(x - new_x) == abs(y - new_y))
            {
                int x_i = (x - new_x < 0) ? 1 : -1;
                int y_i = (y - new_y < 0) ? 1 : -1;
                int rez = abs(x - new_x) - 1;
                for(int i = 0; i <= rez; i++)
                {
                    possible_blocker = this->is_hitted(this->get(x, y), color);
                    if(
                        possible_blocker != NULL
                        && (this->check_king_dependency(possible_blocker, this->get(x, y)) == NULL)
                    )
                        return true;
                    x += x_i;
                    y += y_i;
                }
            }
        }
        if((checker->get_type() == Rook) || (checker->get_type() == Queen))
        {
            if(x == new_x)
            {
                for(int i = min(y, new_y); i <= max(y, new_y); i++)
                {
                    possible_blocker = this->is_hitted(this->get(x, i), color);
                    if(
                        possible_blocker != NULL
                        && (this->check_king_dependency(possible_blocker, this->get(x, i)) == NULL)
                    )
                        return true;
                }
            }
            if(y == new_y)
            {
                for(int i = min(x, new_x); i <= max(x, new_x); i++)
                {
                    possible_blocker = this->is_hitted(this->get(i, y), color);
                    if(
                        possible_blocker != NULL
                        && (this->check_king_dependency(possible_blocker, this->get(i, y)) == NULL)
                    )
                        return true;
                }
            }
        }
        return false;
    }
    Object* check_chess_check(Color color)
    {
        return this->is_hitted((color == WHITE) ? this->get_white_king() : this->get_black_king());
    }
    bool check_mate(Color color)
    {
        Object* checker = check_chess_check(color);
        if(checker == NULL) return false;
        Object* king = ((color == WHITE) ? this->get_white_king() : this->get_black_king());
        int x = king->get_x();
        int y = king->get_y();
        if(double_check == false)
        {
            if(this->check_king_overlap(king, checker))
                return false;
        }
        double_check = false;
        if(
            king->is_legal(this->get(x, y+1), this)
            || king->is_legal(this->get(x+1, y+1), this)
            || king->is_legal(this->get(x+1, y), this)
            || king->is_legal(this->get(x+1, y-1), this)
            || king->is_legal(this->get(x, y-1), this)
            || king->is_legal(this->get(x-1, y-1), this)
            || king->is_legal(this->get(x-1, y), this)
            || king->is_legal(this->get(x-1, y+1), this)
        )   
            return false;

        return true;
    }
    Object* get_white_king() { return white_king; }
    Object* get_black_king() { return black_king; }
    void create_notation_turns_table()
    {
        int first_c;
        int new_x, new_y;
        Object* obj_from;
        Object* obj_to;
        Color color;
        Obj type;
        int pos;
        int size;
        int x_hint;
        int y_hint;
        bool extra;
        for(int i = 0; i < notation_turns.size(); i++)
        {
            color = (i % 2 == 0 ? WHITE : BLACK);
            if((notation_turns[i] == "O-O") || (notation_turns[i] == "O-O-O"))
            {
                int yy = (color == WHITE ? 0 : 7);
                int xx = (notation_turns[i] == "O-O" ? 6 : 2);
                obj_from = this->get(4, yy);
                obj_to = this->get(xx, yy);
                if((obj_from->get_type() == King) && (obj_from->get_color() == color))
                {
                    if(!obj_from->is_legal(obj_to, this))
                        obj_from = NULL;
                    else
                    {
                        extra = true;
                        extra_turns.push_back(
                            new Turn(
                                this->get((notation_turns[i] == "O-O" ? 7 : 0), yy),
                                this->get((notation_turns[i] == "O-O" ? 5 : 3), yy)
                            )
                        );
                    }
                }
                else obj_from = NULL;
            }
            else
            {
                x_hint=y_hint=-1;
                size = notation_turns[i].size();
                pos = 0;
                first_c = notation_turns[i].at(pos++);
                if(first_c == 'R')
                    type = Rook;
                else if(first_c == 'N')
                    type = Knight;
                else if(first_c == 'B')
                    type = Bishop;
                else if(first_c == 'Q')
                    type = Queen;
                else if(first_c == 'K')
                    type = King;
                else
                {
                    type = Pawn;
                    pos = 0;
                }
                if(size >= 4)
                {
                    if(isdigit(notation_turns[i].at(pos)))
                        y_hint = notation_turns[i].at(pos++) - '0' - 1;
                    else
                        x_hint = is_in(notation_turns[i].at(pos++), low_alphabet);
                }
                if(notation_turns[i].at(pos) == 'x') pos++;
                new_x = is_in(notation_turns[i].at(pos++), low_alphabet);
                new_y = notation_turns[i].at(pos++) - '0' - 1;
                obj_to = this->get(new_x, new_y);
                obj_from = this->is_hitted(obj_to, color, type, x_hint, y_hint);
            }
            if(obj_from == NULL)
            {
                cout << "\nIncorrect Notation!" << "\n";
                return;
            }
            turns.push_back(new Turn(obj_from, obj_to));
            turn++;
            if(extra)
            {
                if(cur_state == EnPassant)
                {
                    extra_turns.push_back(
                        new Turn(
                            this->get(obj_to->get_x(), (obj_from->get_y())),
                            this->get(obj_to->get_x(), (obj_from->get_y()))
                        )
                    );
                    cur_state = Nothing;
                }
                turns.at(turn)->set_extra_index(free_extra_index++);
                extra = false;
            }
            make_move_forward(turns.at(turn));
        }
        while(turn >= 0)
        {    
            make_move_backward(turns.at(turn));
            turn--; 
        }
    }
    void make_move_forward(Turn* cur_turn)
    {
        Object* obj_from = cur_turn->get_from();
        Object* obj_to = cur_turn->get_to();
        int x = obj_from->get_x();
        int y = obj_from->get_y();
        int new_x = obj_to->get_x();
        int new_y = obj_to->get_y();
        obj_from->set_x(new_x);
        obj_from->set_y(new_y);
        obj_to->set_x(x);
        obj_to->set_y(y);
        this->add_wd(obj_from);
        this->add_wd(cur_turn->get_replace());
        obj_from->inc_links();
        if(cur_turn->get_extra_index() != -1)
        {
            make_move_forward(extra_turns.at(cur_turn->get_extra_index()));
            if(obj_from->get_color() == WHITE)
                white_castling = true;
            else
                black_castling = true;
        }
    }
    void make_move_backward(Turn* cur_turn)
    {
        Object* obj_from = cur_turn->get_from();
        Object* obj_to = cur_turn->get_to();
        int x = obj_from->get_x();
        int y = obj_from->get_y();
        int new_x = obj_to->get_x();
        int new_y = obj_to->get_y();
        obj_from->set_x(new_x);
        obj_from->set_y(new_y);
        obj_to->set_x(x);
        obj_to->set_y(y);
        this->add_wd(obj_from);
        this->add_wd(obj_to);
        obj_from->dec_links();
        if(cur_turn->get_extra_index() != -1)
        {
            make_move_backward(extra_turns.at(cur_turn->get_extra_index()));
            if(obj_from->get_color() == WHITE)
                white_castling = false;
            else
                black_castling = false;
        }
    }
    void set_menu()
    {
        game_info[0] = "Domz0t's chess\n";
        game_info[1] = "\n";
        game_info[2] = "\n";
        game_info[3] = "Press \"c\" to play classic chess\n";
        game_info[4] = "Press \"l\" to load game\n";
        game_info[5] = "Press \"f\" to flip the board\n";
        game_info[6] = "\n";
        game_info[7] = "\n";
        game_info[8] = "Press \"e\" to exit\n";
    }
    void set_classic_chess_menu()
    {
        game_info[0] = "Domz0t's chess\n";
        game_info[1] = "\n";
        game_info[2] = "\n";
        game_info[3] = "Classic Chess\n";
        game_info[4] = "\n";
        game_info[5] = "\n";
        game_info[6] = "\n";
        game_info[7] = "Press \"b\" to back menu\n";
        game_info[8] = "Press \"e\" to exit\n";
    }
    void activate_start_loading()
    {
        Highlight hl(0, 0, (char*)"\x1b[47m");
        Highlight hl_1(0, 0, (char*)"\x1b[42m");
        Highlight hl_2(0, 0, (char*)"\x1b[43m");
        Highlight hl_3(7, 0, (char*)"\x1b[44m");
        Highlight hl_4(0, 7, (char*)"\x1b[45m");
        Highlight hl_5(0, 0, (char*)"\x1b[46m");
        Highlight hl_6(7, 7, (char*)"\x1b[41m");
        Highlight hl_7(0, 0, (char*)"\x1b[40m");
        this->add_highliter(&hl);
        this->add_highliter(&hl_1);
        this->add_highliter(&hl_2);
        this->add_highliter(&hl_3);
        this->add_highliter(&hl_4);
        this->add_highliter(&hl_5);
        this->add_highliter(&hl_6);
        this->add_highliter(&hl_7);
        for(int i = 0; i < height; i++)
        {
            hl.set_x(i);
            hl.set_y(i);
            hl_1.set_x(height-i-1);
            hl_1.set_y(i);
            hl_2.set_y(i);
            hl_3.set_y(i);
            hl_4.set_x(i);
            hl_5.set_x(height-i-1);
            hl_6.set_x(height-i-1);
            hl_7.set_x(i);
            this->print_board();
            usleep(100000);
        }
        hl.set_y(0);
        hl_1.set_y(1);
        hl_2.set_y(2);
        hl_3.set_y(3);
        hl_4.set_y(4);
        hl_5.set_y(5);
        hl_6.set_y(6);
        hl_7.set_y(7);
        for(int i = 0; i < height; i++)
        {
            hl.set_x(i);
            hl_1.set_x(i);
            hl_2.set_x(i);
            hl_3.set_x(i);
            hl_4.set_x(i);
            hl_5.set_x(i);
            hl_6.set_x(i);
            hl_7.set_x(i);
            this->print_board();
            usleep(50000);
        }
        this->pop_last_highliter();
        this->pop_last_highliter();
        this->pop_last_highliter();
        this->pop_last_highliter();
        this->pop_last_highliter();
        this->pop_last_highliter();
        this->pop_last_highliter();
        this->pop_last_highliter();
    }
    void clear_game_info()
    {
        while(turn >= 0)
        {    
            make_move_backward(turns.at(turn));
            turn--; 
        }
        for(auto i : turns)
        {
            delete i->get_replace();
            delete i;
        }
        for(auto i : extra_turns)
        {
            delete i->get_replace();
            delete i;
        }
        turns.clear();
        extra_turns.clear();
        notation_turns.clear();
        hl_v.clear();
        free_extra_index = 0;
        cur_state = Nothing;
        white_castling = false;
        black_castling = false;
    }
    void start()
    {
        regime = Menu;
        cur_state = Nothing;
        int temp;
        int x = 4, y = 1;
        int temp_x, temp_y;
        set_input_mode();

        set_menu();
        activate_start_loading();

        Highlight hl1(-1, -1, (char*)"\x1b[47m");
        Highlight hl2(-1, -1, (char*)"\x1b[42m");

        this->print_board(); 
        cout << "\n";

        bool enter_firstly_pressed = false;
        Object* obj_from;
        Object* obj_to;

        while(true)
        {
            while(-1 == read(STDIN_FILENO, &temp, 1));
            if((temp == 'w') && (regime == Classic))
                if(board_flipped)
                    y--;
                else
                    y++;
            else if((temp == 's') && (regime == Classic))
                if(board_flipped)
                    y++;
                else
                    y--;
            else if((temp == 'd') && (regime == Classic))
                if(board_flipped)
                    x--;
                else
                    x++;
            else if((temp == 'a') && (regime == Classic))
                if(board_flipped)
                    x++;
                else
                    x--;
            else if(temp == 'f')
                board_flipped = !board_flipped;
            else if(temp == 'b')
            {
                if(regime != Menu)
                {
                    clear_game_info();
                    regime = Menu;
                    set_menu();
                }
            }
            else if(temp == 'e')
            {
                return;
            }
            else if(temp == 65)
            {
                if(turns.size() != 0)
                {
                    int size = turns.size() - 1;
                    while(turn < size)
                    {
                        turn++;
                        make_move_forward(turns.at(turn));
                    }
                    hl1.set_x(turns.at(turn)->get_from()->get_x());
                    hl1.set_y(turns.at(turn)->get_from()->get_y());
                    hl2.set_x(turns.at(turn)->get_to()->get_x());
                    hl2.set_y(turns.at(turn)->get_to()->get_y());
                }
            }
            else if(temp == 66)
            {
                while(turn > 0)
                {
                    make_move_backward(turns.at(turn));
                    turn--;
                }
                if(turn != -1)
                {
                    hl1.set_x(turns.at(turn)->get_from()->get_x());
                    hl1.set_y(turns.at(turn)->get_from()->get_y());
                    hl2.set_x(turns.at(turn)->get_to()->get_x());
                    hl2.set_y(turns.at(turn)->get_to()->get_y());
                }
            }
            else if(temp == 67)
            {
                if(turns.size() != 0)
                {
                    turn++;
                    if(turn > notation_turns.size()-1)
                        turn = range(turn, -1, notation_turns.size()-1);
                    else
                    {
                        make_move_forward(turns.at(turn));
                        hl1.set_x(turns.at(turn)->get_from()->get_x());
                        hl1.set_y(turns.at(turn)->get_from()->get_y());
                        hl2.set_x(turns.at(turn)->get_to()->get_x());
                        hl2.set_y(turns.at(turn)->get_to()->get_y());
                    }
                }
            }
            else if(temp == 68)
            {
                if(turn < 0)
                    turn = range(turn, -1, notation_turns.size()-1);
                else
                {
                    make_move_backward(turns.at(turn));
                    hl1.set_x(turns.at(turn)->get_from()->get_x());
                    hl1.set_y(turns.at(turn)->get_from()->get_y());
                    hl2.set_x(turns.at(turn)->get_to()->get_x());
                    hl2.set_y(turns.at(turn)->get_to()->get_y());
                    turn--;
                }
            }
            else if(temp == 'l')
            {
                reset_input_mode();
                string str;
                cout << "Enter path to game: " << "\n";
                cin >> str;
                ifstream f;
                f.open(str);
                if(f.is_open())
                {
                    clear_game_info();
                    int pos = 0;
                    regex r("([A-Za-z]+[0-9])|((O-)+O)");
                    while(getline(f, str) && (pos != 10))
                    {
                        game_info[pos] = str;
                        if(pos == 8)
                            game_info[pos] += "\t\tPress \"b\" to back menu";
                        game_info[pos++] += "\n";
                    }
                    if(!regex_search(str, r) || (pos != 10))
                    {
                        cout << "\nIncorrect File!" << "\n";
                        skip = true;
                    }
                    else
                    {
                        smatch m;
                        while(regex_search(str, m, r))
                        {
                            notation_turns.push_back(m.str());
                            str = m.suffix();
                        }
                        create_notation_turns_table();
                        this->add_highliter(&hl1);
                        this->add_highliter(&hl2);
                        regime = View;
                    }
                }
                else
                {
                    skip = true;
                    cout << "\nFile doesn't exist!" << "\n";
                }  
                f.close();
                cin.clear(); 
                cin.ignore(numeric_limits<streamsize>::max(), '\n');   
                set_input_mode();
            }
            else if((temp == 10) && (regime == Classic))
            {
                if(enter_firstly_pressed == false)
                {
                    if((this->get(x, y)->get_color() == (((turn + 1) % 2 == 0) ? WHITE : BLACK)) && (this->get(x, y)->get_type() != Obj::Square))
                    {
                        hl2.set_x(x);
                        hl2.set_y(y);
                        this->add_highliter(&hl2);
                        enter_firstly_pressed = true;
                        temp_x = x;
                        temp_y = y;
                    }
                }
                else
                {
                    bool tumbler = false;
                    Turn* answer;

                    obj_from = this->get(temp_x, temp_y);
                    obj_to = this->get(x, y);
                    l1:
                    // if(tumbler == false)
                    // {
                    //     obj_from = answer->get_from();
                    //     obj_to = answer->get_to();
                    // }
                    if(obj_from->is_legal(obj_to, this)
                    && (this->check_king_dependency(obj_from, obj_to) == NULL))
                    {
                        string str = "";
                        if((cur_state != ShortCastling) && (cur_state != LongCastling))
                        {
                            if(obj_from->get_type() == King)
                                str += "K";
                            else if(obj_from->get_type() == Queen)
                                str += "Q";
                            else if(obj_from->get_type() == Knight)
                                str += "N";
                            else if(obj_from->get_type() == Bishop)
                                str += "B";
                            else if(obj_from->get_type() == Rook)
                                str += "R";
                            str += low_alphabet[obj_to->get_x()];
                            str += ('1' + obj_to->get_y());
                        }
                        int high_border = turns.size() - 1;
                        int low_border = turn;
                        for(int i = high_border; i > low_border; i--)
                        {
                            if(turns.at(i)->get_extra_index() != -1)
                            {
                                free_extra_index--;
                                delete extra_turns.at(free_extra_index)->get_replace();
                                delete extra_turns.at(free_extra_index);
                                extra_turns.pop_back();
                            }
                            delete turns.at(i)->get_replace();
                            delete turns.at(i);
                            turns.pop_back();
                            notation_turns.pop_back();
                        }
                        turns.push_back(new Turn(obj_from, obj_to));
                        turn++;
                        if(cur_state != Nothing)
                        {
                            if(cur_state == EnPassant)
                            {
                                extra_turns.push_back(
                                    new Turn(
                                        this->get(obj_to->get_x(), (obj_from->get_y())),
                                        this->get(obj_to->get_x(), (obj_from->get_y()))
                                    )
                                );
                            }
                            else
                            {
                                int yy = ((obj_from->get_color() == WHITE) ? 0 : 7);
                                extra_turns.push_back(
                                    new Turn(
                                        this->get(((cur_state == ShortCastling) ? 7 : 0), yy),
                                        this->get(((cur_state == ShortCastling) ? 5 : 3), yy)
                                    )
                                );
                                str += ((cur_state == ShortCastling) ? "O-O" : "O-O-O");
                            }
                            turns.at(turn)->set_extra_index(free_extra_index++);
                            cur_state = Nothing;
                        }
                        make_move_forward(turns.at(turn));
                        notation_turns.push_back(str);  
                        if(tumbler)
                        {
                            ai.analyze(this, (((turn + 1) % 2 == 0) ? WHITE : BLACK));
                            tumbler = false;
                            goto l1;
                        }

                    }
                    else
                    {
                        x = temp_x;
                        y = temp_y;
                    }
                    this->pop_last_highliter();
                    enter_firstly_pressed = false;
                }
            }
            else if(temp == 'c')
            {
                clear_game_info();
                set_classic_chess_menu();
                x = 4;
                y = 1;
                this->add_highliter(&hl1);
                enter_firstly_pressed = false;
                regime = Classic;
            }
            else if(temp == 'i')
                AI_state = true;
            if(regime == Classic)
            {   
                x = range(x, 0, 7);
                y = range(y, 0, 7);
                hl1.set_x(x);
                hl1.set_y(y);
            }

            if(skip)
            {
                skip = false;
                continue;
            }

            this->print_board(); 
            for(int i = 0; i < turns.size(); i++)
            {
                if((i) % 12 == 0) cout << "\n";
                if(i % 2 == 0)
                    cout << i/2 + 1 << ".";
                if(i == (turn))
                    cout << "\x1b[47m";
                cout << notation_turns.at(i) << " ";
                cout << "\x1b[0m";
            }
            cout << "\n";
            if(this->check_mate(WHITE)) cout << "White king is mated!" << "\n";
            else if(this->check_chess_check(WHITE)) cout << "White king is checked!" << "\n";
            else if(this->check_mate(BLACK)) cout << "Black king is mated!" << "\n";
            else if(this->check_chess_check(BLACK)) cout << "Black king is checked!" << "\n";
            if(AI_state)
            {
                ai.analyze(this, (((turn + 1) % 2 == 0) ? WHITE : BLACK));
                AI_state = false;
            }
        }
    }
    ~Board()
    {
        clear_game_info();
        for(int i = 0; i < height* width; i++)
            delete board[i];
        delete [] board;
    }
};

class Pawn : public Object
{
    public:
    Pawn(int _x, int _y, Color _clr) : Object(_x, _y, _clr)
    {
        if(this->get_color() == WHITE)
            this->set_img((char*)"\u2659");
        else
            this->set_img((char*)"\u265F");
        this->set_type(Obj::Pawn);
    }
    virtual bool is_legal(Object* obj, Board* board) 
    {
        int x = this->get_x();
        int y = this->get_y();
        int new_x = obj->get_x();
        int new_y = obj->get_y();

        if(x == new_x && y == new_y) return false;
        if((obj->get_type() != Square) && (obj->get_color() == this->get_color())) return false;

        if(this -> get_color() == WHITE)
        {
            if(new_x == x)
            {
                if(new_y == y + 1)
                {
                    return (obj->get_type() == Obj::Square);
                }
                if(y == 1 && new_y == 3 && (obj->get_type() == Obj::Square) && (board->get(x, 2)->get_type() == Obj::Square))
                {
                    board->set_hit_field(board->get(x, 2));
                    board->get(x, 2)->set_extra(board->get_turn());
                    return true;
                }
            }
            else if(abs(new_x - x) == 1)
            {
                if(new_y == y + 1)
                {
                    if((obj->get_type() != Obj::Square) && (obj->get_color() == BLACK)) 
                        return true;
                    if(
                        (board->get_hit_field() != NULL) &&
                        (obj->get_x() == board->get_hit_field()->get_x()) &&
                        (obj->get_y() == board->get_hit_field()->get_y()) &&
                        (board->get_turn() == (board->get_hit_field()->get_extra() + 1))
                    ) 
                    {
                        board->set_cur_state(EnPassant);
                        return true;
                    }
                }
            }
        }
        else
        {
            if(new_x == x)
            {
                if(new_y == y - 1)
                {
                    return (obj->get_type() == Obj::Square);
                }
                if(y == 6 && new_y == 4 && (obj->get_type() == Obj::Square) && (board->get(x, 5)->get_type() == Obj::Square))
                {
                    board->set_hit_field(board->get(x, 5));
                    board->get(x, 5)->set_extra(board->get_turn());
                    return true;
                }
            }
            else if(abs(new_x - x) == 1)
            {
                if(new_y == y - 1)
                {
                    if((obj->get_type() != Obj::Square) && (obj->get_color() == WHITE)) 
                        return true;
                    if(
                        (board->get_hit_field() != NULL) &&
                        (obj->get_x() == board->get_hit_field()->get_x()) &&
                        (obj->get_y() == board->get_hit_field()->get_y()) &&
                        (board->get_turn() == (board->get_hit_field()->get_extra() + 1))
                    ) 
                    {
                        board->set_cur_state(EnPassant);
                        return true;
                    }
                }
            }
        }
        return false;
    }

};

class Rook : public Object
{
    public:
    Rook(int _x, int _y, Color _clr) : Object(_x, _y, _clr)
    {
        if(this->get_color() == WHITE)
            this->set_img((char*)"\u2656");
        else
            this->set_img((char*)"\u265C");
        this->set_type(Obj::Rook);
    }
    virtual bool is_legal(Object* obj, Board* board) 
    {
        int x = this->get_x();
        int y = this->get_y();
        int new_x = obj->get_x();
        int new_y = obj->get_y();

        if(x == new_x && y == new_y) return false;
        if((obj->get_type() != Square) && (obj->get_color() == this->get_color())) return false;
        if(x == new_x)
        {
            for(int i = min(y, new_y) + 1; i < max(y, new_y); i++)
            {
                if(board->get(x, i)->get_type() != Square)
                    return false;
            }
            return true;
        }
        else
        if(y == new_y)
        {
            for(int i = min(x, new_x) + 1; i < max(x, new_x); i++)
            {
                if(board->get(i, y)->get_type() != Square)
                    return false;
            }
            return true;
        }
        return false;
    }

};

class Bishop : public Object
{
    public:
    Bishop(int _x, int _y, Color _clr) : Object(_x, _y, _clr)
    {
        if(this->get_color() == WHITE)
            this->set_img((char*)"\u2657");
        else
            this->set_img((char*)"\u265D");
        this->set_type(Obj::Bishop);
    }
    virtual bool is_legal(Object* obj, Board* board) 
    {
        int x = this->get_x();
        int y = this->get_y();
        int new_x = obj->get_x();
        int new_y = obj->get_y();

        if(x == new_x && y == new_y) return false;
        if((obj->get_type() != Square) && (obj->get_color() == this->get_color())) return false;
        if(abs(x - new_x) == abs(y - new_y))
        {
            int x_i = (x - new_x < 0) ? 1 : -1;
            int y_i = (y - new_y < 0) ? 1 : -1;
            int rez = abs(x - new_x) - 1;
            for(int i = 0; i < rez; i++)
            {
                x += x_i;
                y += y_i;
                if(board->get(x, y)->get_type() != Square)
                    return false;
            }
            return true;
        }
        return false;
    }

};

class Knight : public Object
{
    public:
    Knight(int _x, int _y, Color _clr) : Object(_x, _y, _clr)
    {
        if(this->get_color() == WHITE)
            this->set_img((char*)"\u2658");
        else
            this->set_img((char*)"\u265E");
        this->set_type(Obj::Knight);
    }
    virtual bool is_legal(Object* obj, Board* board) 
    {
        int x = this->get_x();
        int y = this->get_y();
        int new_x = obj->get_x();
        int new_y = obj->get_y();

        if(x == new_x && y == new_y) return false;
        if((obj->get_type() != Square) && (obj->get_color() == this->get_color())) return false;
        if((abs(x - new_x) == 2 && abs(y - new_y) == 1) || (abs(x - new_x) == 1 && abs(y - new_y) == 2))
            return true;
        return false;
    }

};

class Queen : public Object
{
    public:
    Queen(int _x, int _y, Color _clr) : Object(_x, _y, _clr)
    {
        if(this->get_color() == WHITE)
            this->set_img((char*)"\u2655");
        else
            this->set_img((char*)"\u265B");
        this->set_type(Obj::Queen);
    }
    virtual bool is_legal(Object* obj, Board* board) 
    {
        int x = this->get_x();
        int y = this->get_y();
        int new_x = obj->get_x();
        int new_y = obj->get_y();

        if(x == new_x && y == new_y) return false;
        if((obj->get_type() != Square) && (obj->get_color() == this->get_color())) return false;
        if(x == new_x)
        {
            for(int i = min(y, new_y) + 1; i < max(y, new_y); i++)
            {
                if(board->get(x, i)->get_type() != Square)
                    return false;
            }
            return true;
        }
        else
        if(y == new_y)
        {
            for(int i = min(x, new_x) + 1; i < max(x, new_x); i++)
            {
                if(board->get(i, y)->get_type() != Square)
                    return false;
            }
            return true;
        }
        else
        if(abs(x - new_x) == abs(y - new_y))
        {
            int x_i = (x - new_x < 0) ? 1 : -1;
            int y_i = (y - new_y < 0) ? 1 : -1;
            int rez = abs(x - new_x) - 1;
            for(int i = 0; i < rez; i++)
            {
                x += x_i;
                y += y_i;
                if(board->get(x, y)->get_type() != Square)
                    return false;
            }
            return true;
        }
        return false;
    }

};

class King : public Object
{
    public:
    King(int _x, int _y, Color _clr) : Object(_x, _y, _clr)
    {
        if(this->get_color() == WHITE)
            this->set_img((char*)"\u2654");
        else
            this->set_img((char*)"\u265A");
        this->set_type(Obj::King);
    }
    virtual bool is_legal(Object* obj, Board* board)  
    {
        if(obj == NULL) return false;
        int x = this->get_x();
        int y = this->get_y();
        int new_x = obj->get_x();
        int new_y = obj->get_y();
        Color color = this->get_color();

        if(x == new_x && y == new_y) return false;
        if((obj->get_type() != Square) && (obj->get_color() == this->get_color())) return false;
        if(abs(x - new_x) <= 1 && abs(y - new_y) <= 1)
        {
            if(board->check_king_dependency(this, obj) == NULL)
                return true;
        }
        //King and Rook
        int yy = (color == WHITE ? 0 : 7);
        if(x == 4 && y == yy && (this->get_links() == 0))
        {
            Object* right_rook = board->get(7, yy);
            Object* left_rook = board->get(0, yy);
            if(
                (new_x == 6 && new_y == yy) && (board->get(5, yy)->get_type() == Square) 
                && (board->get(6, yy)->get_type() == Square) && (right_rook->get_type() == Rook) 
                && (right_rook->get_color() == color) && (right_rook->get_links() == 0)
                && (board->is_hitted(this, (color == WHITE ? BLACK : WHITE)) == NULL)
                && (board->is_hitted(board->get(5, yy), (color == WHITE ? BLACK : WHITE)) == NULL)
                && (board->is_hitted(board->get(6, yy), (color == WHITE ? BLACK : WHITE)) == NULL)
            )
            {
                board->set_cur_state(ShortCastling);
                return true;
            }
            if( 
                (new_x == 2 && new_y == yy) && (board->get(1, yy)->get_type() == Square) 
                && (board->get(2, yy)->get_type() == Square) && (board->get(3, yy)->get_type() == Square)
                && (left_rook->get_type() == Rook) 
                && (left_rook->get_color() == color) && (left_rook->get_links() == 0)
                && !board->is_hitted(this, (color == WHITE ? BLACK : WHITE))
                && (board->is_hitted(board->get(3, yy), (color == WHITE ? BLACK : WHITE)) == NULL)
                && (board->is_hitted(board->get(2, yy), (color == WHITE ? BLACK : WHITE)) == NULL)
            )
            {
                board->set_cur_state(LongCastling);
                return true;
            }
        }
        return false;
    }

};

void Board::set_start_position()
{
    this->clear();
    this->add(new class Rook(0, 0, WHITE));
    this->add(new class Rook(0, 7, BLACK));
    this->add(new class Knight(1, 0, WHITE));
    this->add(new class Knight(1, 7, BLACK));
    this->add(new class Bishop(2, 0, WHITE));
    this->add(new class Bishop(2, 7, BLACK));
    this->add(new class Queen(3, 0, WHITE));
    this->add(new class Queen(3, 7, BLACK));
    this->add(new class King(4, 0, WHITE));
    this->add(new class King(4, 7, BLACK));
    this->add(new class Bishop(5, 0, WHITE));
    this->add(new class Bishop(5, 7, BLACK));
    this->add(new class Knight(6, 0, WHITE));
    this->add(new class Knight(6, 7, BLACK));
    this->add(new class Rook(7, 0, WHITE));
    this->add(new class Rook(7, 7, BLACK));
    for(int i = 0; i < 8; i++)
    {
        this->add(new class Pawn(i, 1, WHITE));
        this->add(new class Pawn(i, 6, BLACK));
    }
}


double AI::check_mobility(Object* obj, Board* board)
{
    int hitted_fields = 0;
    for(int i = 0; i < 64; i++)
            if(obj->is_legal(board->board[i], board)) hitted_fields++;
    return hitted_fields;
}
bool AI::check_passed_pawn(Object* obj, Board* board)
{
    int x = obj->get_x();
    if(obj->get_color() == WHITE)
    {
        for(int i = obj->get_y() + 1; i < 7; i++)
        {
            if(
                ((board->get(x - 1, i) != NULL) && (board->get(x - 1, i)->get_type() == Pawn) && (board->get(x - 1, i)->get_color() == BLACK)) ||
                ((board->get(x, i) != NULL) && (board->get(x, i)->get_type() == Pawn) && (board->get(x, i)->get_color() == BLACK)) ||
                ((board->get(x + 1, i) != NULL) && (board->get(x + 1, i)->get_type() == Pawn) && (board->get(x + 1, i)->get_color() == BLACK))
            )
                return false;
        }
    }
    else
    {
        for(int i = obj->get_y() - 1; i > 0; i--)
        {
            if(
                ((board->get(x - 1, i) != NULL) && (board->get(x - 1, i)->get_type() == Pawn) && (board->get(x - 1, i)->get_color() == WHITE)) ||
                ((board->get(x, i) != NULL) && (board->get(x, i)->get_type() == Pawn) && (board->get(x, i)->get_color() == WHITE)) ||
                ((board->get(x + 1, i) != NULL) && (board->get(x + 1, i)->get_type() == Pawn) && (board->get(x + 1, i)->get_color() == WHITE))
            )
                return false;
        }
    }
    return true;
}
double AI::evaluate_best_answer(Board* board, Color turn_color, int depth)
{
    vector<Turn*> possible_turns;
    Object* obj_from;
    Object* obj_to;
    Turn* temp_turn;
    double evaluation;
    for(int i = 0; i < 64; i++)
    {
        obj_from = board->board[i];
        if(obj_from->get_type() == Square) continue;
        if(obj_from->get_color() != turn_color) continue;
        for(int j = 0; j < 64; j++)
        {
            obj_to = board->board[j];
            if(obj_from->is_legal(obj_to, board)
            && (board->check_king_dependency(obj_from, obj_to) == NULL))
            {
                temp_turn = new Turn(obj_from, obj_to);
                if(board->cur_state != Nothing)
                {
                    if(board->cur_state == EnPassant)
                    {
                        board->extra_turns.push_back(
                            new Turn(
                                board->get(obj_to->get_x(), (obj_from->get_y())),
                                board->get(obj_to->get_x(), (obj_from->get_y()))
                            )
                        );
                    }
                    else
                    {
                        int yy = ((obj_from->get_color() == WHITE) ? 0 : 7);
                        board->extra_turns.push_back(
                            new Turn(
                                board->get(((board->cur_state == ShortCastling) ? 7 : 0), yy),
                                board->get(((board->cur_state == ShortCastling) ? 5 : 3), yy)
                            )
                        );
                    }
                    temp_turn->set_extra_index(board->free_extra_index++);
                    board->cur_state = Nothing;
                }
                board->make_move_forward(temp_turn);
                if(depth > 0)
                    temp_turn->set_ai_evaluation(evaluate_best_answer(board, reverse_color(turn_color), depth-1));
                else
                    temp_turn->set_ai_evaluation(static_analyze(board));
                board->make_move_backward(temp_turn);
                possible_turns.push_back(temp_turn);
            }
        }
    }
    if(turn_color == WHITE)
        sort(possible_turns.begin(), possible_turns.end(), compareTurnsForWhite);
    else
        sort(possible_turns.begin(), possible_turns.end(), compareTurnsForBlack);   
    evaluation = possible_turns.at(0)->get_ai_evaluation();
    for(auto turn : possible_turns)
    {
        if(turn->get_extra_index() != -1)
        {
            board->free_extra_index--;
            delete board->extra_turns.at(board->free_extra_index)->get_replace();
            delete board->extra_turns.at(board->free_extra_index);
            board->extra_turns.pop_back();
        }
        delete turn->get_replace();
        delete turn;
    }
    return evaluation;
}
Turn* AI::analyze(Board* board, Color turn_color)
{
    vector<Turn*> possible_turns;
    Object* obj_from;
    Object* obj_to;
    Turn* temp_turn;
    Turn* result;   
    cout << "Analyzing" << endl;
    for(int i = 0; i < 64; i++)
    {
        obj_from = board->board[i];
        if(obj_from->get_type() == Square) continue;
        if(obj_from->get_color() != turn_color) continue;
        for(int j = 0; j < 64; j++)
        {
            obj_to = board->board[j];
            if(obj_from->is_legal(obj_to, board)
            && (board->check_king_dependency(obj_from, obj_to) == NULL))
            {
                temp_turn = new Turn(obj_from, obj_to);
                if(board->cur_state != Nothing)
                {
                    if(board->cur_state == EnPassant)
                    {
                        board->extra_turns.push_back(
                            new Turn(
                                board->get(obj_to->get_x(), (obj_from->get_y())),
                                board->get(obj_to->get_x(), (obj_from->get_y()))
                            )
                        );
                    }
                    else
                    {
                        int yy = ((obj_from->get_color() == WHITE) ? 0 : 7);
                        board->extra_turns.push_back(
                            new Turn(
                                board->get(((board->cur_state == ShortCastling) ? 7 : 0), yy),
                                board->get(((board->cur_state == ShortCastling) ? 5 : 3), yy)
                            )
                        );
                    }
                    temp_turn->set_extra_index(board->free_extra_index++);
                    board->cur_state = Nothing;
                }
                board->make_move_forward(temp_turn);
                temp_turn->set_ai_evaluation(evaluate_best_answer(board, reverse_color(turn_color), 1));
                board->make_move_backward(temp_turn);
                possible_turns.push_back(temp_turn);
                if(possible_turns.size() % 7 == 0)
                {
                    cout << "\033[F";
                    cout << "Analyzing";
                    for(int k = 0; k < possible_turns.size() / 7; k++)
                        cout << ".";
                    cout << endl;
                }
            }
        }
    }
    if(turn_color == WHITE)
        sort(possible_turns.begin(), possible_turns.end(), compareTurnsForWhite);
    else
        sort(possible_turns.begin(), possible_turns.end(), compareTurnsForBlack);

    int border = min(5, (int)possible_turns.size());
    cout << "\033[F";
    for(int i = 0; i < border; i++)
        cout << "Top " << i + 1 << ": " 
        << board->low_alphabet[possible_turns.at(i)->get_from()->get_x()] << possible_turns.at(i)->get_from()->get_y() + 1 
        << " -> "
        << board->low_alphabet[possible_turns.at(i)->get_to()->get_x()] << possible_turns.at(i)->get_to()->get_y() + 1
        <<  " " << possible_turns.at(i)->get_ai_evaluation() << "\n";
    if(possible_turns.size() == 0) return NULL;
    result = possible_turns.at(0);
    for(int i = 1; i < possible_turns.size(); i++)
    {
        Turn* turn = possible_turns.at(i);
        if(turn->get_extra_index() != -1)
        {
            board->free_extra_index--;
            delete board->extra_turns.at(board->free_extra_index)->get_replace();
            delete board->extra_turns.at(board->free_extra_index);
            board->extra_turns.pop_back();
        }
        delete turn->get_replace();
        delete turn;
    }
    return result;
}
double AI::static_analyze(Board* board)
{
    State save_state = board->cur_state;
    Object* save_hit_field = board->hit_field;
    double analyze_rate = 0.0;
    Color color;
    Object* obj;
    Object* temp_obj;
    int white_pawns_on_vert;
    int black_pawns_on_vert;
    int white_bishops = 0;
    int black_bishops = 0;
    for(int x = 0; x < 8; x++)
    {
        black_pawns_on_vert = 0;
        white_pawns_on_vert = 0;
        for(int y = 0; y < 8; y++)
        {
            obj = board->get(x, y);
            color = obj->get_color();
            switch (obj->get_type())
            {
                case Square:
                    break;
                case Pawn:
                    analyze_rate += (color == WHITE ? 1 : -1) * 100;
                    if(color == WHITE)
                    {
                        white_pawns_on_vert++;
                        temp_obj = board->get(obj->get_x() - 1, obj->get_y() - 1);
                        if((temp_obj != NULL) && (temp_obj->get_type() == Pawn) && (temp_obj->get_color() == WHITE)) analyze_rate += 12;
                        temp_obj = board->get(obj->get_x() + 1, obj->get_y() - 1);
                        if((temp_obj != NULL) && (temp_obj->get_type() == Pawn) && (temp_obj->get_color() == WHITE)) analyze_rate += 12;
                        if(check_passed_pawn(obj, board))
                            analyze_rate += passed_pawn_reward[obj->get_y()];
                        else
                            analyze_rate += standard_pawn_reward[obj->get_y()];
                    }
                    else
                    {
                        black_pawns_on_vert++;
                        temp_obj = board->get(obj->get_x() - 1, obj->get_y() + 1);
                        if((temp_obj != NULL) && (temp_obj->get_type() == Pawn) && (temp_obj->get_color() == BLACK)) analyze_rate -= 12;
                        temp_obj = board->get(obj->get_x() + 1, obj->get_y() + 1);
                        if((temp_obj != NULL) && (temp_obj->get_type() == Pawn) && (temp_obj->get_color() == BLACK)) analyze_rate -= 12;
                        if(check_passed_pawn(obj, board))
                            analyze_rate -= passed_pawn_reward[7 - obj->get_y()];
                        else
                            analyze_rate -= standard_pawn_reward[7 - obj->get_y()];
                    }
                    break;
                case Knight:
                    analyze_rate += (color == WHITE ? 1 : -1) * 305;
                    analyze_rate += (color == WHITE ? 1 : -1) * check_mobility(obj, board) * 9;
                    break;
                case Bishop:
                    analyze_rate += (color == WHITE ? 1 : -1) * 333;
                    analyze_rate += (color == WHITE ? 1 : -1) * check_mobility(obj, board) * 4;
                    if(color == WHITE)
                        white_bishops++;
                    else
                        black_bishops++;
                    break;
                case Rook:
                    analyze_rate += (color == WHITE ? 1 : -1) * 563;
                    analyze_rate += (color == WHITE ? 1 : -1) * check_mobility(obj, board) * 3;
                    break;
                case Queen:
                    analyze_rate += (color == WHITE ? 1 : -1) * 950;
                    analyze_rate += (color == WHITE ? 1 : -1) * check_mobility(obj, board) * 3;
                    break;
                case King:
                    if(color == WHITE)
                    {
                        if((obj->get_links() != 0) && (!board->white_castling))
                            analyze_rate -= 50;
                    }
                    else
                    {
                        if((obj->get_links() != 0) && (!board->black_castling))
                            analyze_rate += 50;
                    }
                default:
                    break;
            } 
        }
        if(white_pawns_on_vert != 0)
            analyze_rate += (white_pawns_on_vert - 1) * (-25);
        if(black_pawns_on_vert != 0)
            analyze_rate += (black_pawns_on_vert - 1) * (25);
    }
    if(white_bishops == 2) analyze_rate += 50;
    if(black_bishops == 2) analyze_rate -= 50;
    
    board->cur_state = save_state;
    board->hit_field = save_hit_field;
    return analyze_rate/100;
}


int main()
{
    Board board;
    board.set_start_position();
    board.start();


    return 0;

}
