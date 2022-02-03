/*
    UMBC CMSC 2021
    SPRING 2021
    PROJECT 3
    
    Due Date: 04/25/2021

    Author Name: Alex Tran
    Author email: ptran7@umbc.edu
*/
#include <linux/ctype.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/semaphore.h>
#include <linux/uaccess.h>
#include <linux/errno.h>
#include <linux/rwsem.h>

#define BOARD_LEN 67
#define ILLMOVE -1
#define INVFMT -2
#define UNKCMD -4
#define NOGAME -3
#define OOT -5
#define MAX_ROWS 8
#define MIN_ROW 0
#define MAX_ROW 7
#define OK 10
#define WIN 16
#define LOSE 17
#define TIE 18

/* PROTOTYPE */
static int __init init_reversi(void);
static void __exit exit_reversi(void);
static int reversi_open(struct inode *, struct file *);
static ssize_t reversi_read(struct file *, char *, size_t,loff_t *);
static ssize_t reversi_write(struct file *, const char *, size_t, loff_t *);
int reversi_close(struct inode *, struct file *);
static int input_val(const char*);
static void new_board(char, int);
static int valid_Move(char,int, int);
static void initial_board(void);
static int valid_cord(int, int);
static int cpu_turn(void);
static int valid_loc(int);
static int pass_turn(void);
static int full_board(void);
static void win_check(void);
MODULE_LICENSE("GPL");

static DECLARE_RWSEM(lock);
static int OPEN = 0;
static int VALID_OPT = 0;
static char game_board[BOARD_LEN];
static char CPU_PIECE;
static char PLAYER_PIECE;
static int RUNNING = 0;
static int ROW;
static int COLUMN;
static int TURN;
static int PLAYER_SCORE;
static int CPU_SCORE;
static int dir_arr[MAX_ROWS][2] = {{-1,-1},{-1,0},{-1,1},{0,-1},{0,1},{1,-1},{1,0},{1,1}};

static const struct file_operations reversi_fops = {
    .open = reversi_open,
    .read = reversi_read,
    .write = reversi_write,
    .release = reversi_close,
};

static struct miscdevice reversi_dev = {
    .minor = 0,
    .name = "reversi",
    .fops = &reversi_fops,
    .mode = 0666,
};

static int reversi_open(struct inode *inode, struct file *fptr){
    if(OPEN){
        printk("The reversi kernel module is currently busy \n");
        return -EBUSY;
    }
    OPEN++;
    printk(KERN_INFO "reversi: opened device \n");
    try_module_get(THIS_MODULE);
    return 0;
}

/* outputs the kernel's response to the action in write */
static ssize_t reversi_read(struct file *fptr, char __user* usrbuff, size_t size, loff_t *offset){
    int error_val; int output_len;
    char * output_str = NULL;
    output_len = 0;

    if(access_ok(usrbuff,size) == 0){
        return -EFAULT;
    }
    /* Returns the current state of the game board. */
    if(VALID_OPT == 3){
        down_read(&lock);
        error_val = copy_to_user(usrbuff,game_board,BOARD_LEN);
        if(error_val != 0){
            up_read(&lock);
            return -ENOMEM;
        }else{
            up_read(&lock);
            return BOARD_LEN;
        }        
    }
    if(VALID_OPT == OK){
        output_str = "OK\n"; 
    }else if(VALID_OPT == INVFMT){
        output_str = "INVFMT\n";
    }else if(VALID_OPT == ILLMOVE){
        output_str = "ILLMOVE\n";
    }else if(VALID_OPT == UNKCMD){
        output_str = "UNKCMD\n";
    }else if(VALID_OPT == NOGAME){
        output_str = "NOGAME\n";
    }else if(VALID_OPT == OOT){
        output_str = "OOT\n";
    }else if(VALID_OPT == WIN){
        output_str = "WIN\n";
        RUNNING = 0;
    }else if(VALID_OPT == LOSE){
        output_str = "LOSE\n";
        RUNNING = 0;
    }else if(VALID_OPT == TIE){
        output_str = "TIE\n";
        RUNNING = 0;
    }
    if(strlen(output_str) < size){
        output_len = strlen(output_str);
    }else{
        output_len = size;
    };
    down_read(&lock);
    error_val = copy_to_user(usrbuff, output_str, output_len);
    if(error_val != 0){
        up_read(&lock);
        return -ENOMEM;
    }else{
        up_read(&lock);
        return output_len;
    }
    up_read(&lock);
    return -EINVAL;
}

/* attempts the actions given by user */
static ssize_t reversi_write(struct file *fptr, const char __user* usrbuff, size_t size, loff_t *offset){
    int val_move;
    if(access_ok(usrbuff,size) == 0){
        return -EFAULT;
    }
    /* validates the user inputs to the exact match of the command */ 
    VALID_OPT = input_val(usrbuff);
    down_write(&lock);

    if(VALID_OPT == 1){                         /* Begin a new game w/ user piece X. Resets the game board to the initial state. */
        new_board('X', 1);
        VALID_OPT = OK;
    }else if (VALID_OPT == 2){                  /* Begin a new game w/ user piece O. Resets the game board to the initial state. */
        new_board('O', 0);
        VALID_OPT = OK;
    }else if(VALID_OPT == 3){
        up_write(&lock);
        return size;                            /* Return in order to print the board */
    }else if (VALID_OPT == 4){
        /* check if it was the player turn*/
        if((TURN == 1)){
            val_move = valid_Move(PLAYER_PIECE, ROW, COLUMN);            
            if(val_move == 0){
                VALID_OPT = ILLMOVE;        
            }else{
                /* update player/cpu score */
                VALID_OPT = OK;
                PLAYER_SCORE += val_move;
                CPU_SCORE -= val_move;
                TURN = 0;
                game_board[65] = CPU_PIECE;
                win_check();
            }  
        }else{
            // set valid opt according if there was an invalid move
            VALID_OPT = OOT;
        }
    }else if (VALID_OPT == 5){
        /*Check if it is the CPU's turn*/ 
        if(TURN == 1){
            VALID_OPT = OOT;
        }else{
            val_move = cpu_turn();
            VALID_OPT = OK;
            TURN = 1;
            PLAYER_SCORE -= val_move;
            CPU_SCORE += val_move;
            game_board[65] = PLAYER_PIECE;
            win_check();
        }
    }else if (VALID_OPT == 6){
        /*Check if it is the player's turn to skip their move*/
        if(TURN == 0){
            VALID_OPT = OOT;
        }else{
            VALID_OPT = pass_turn();
            if(VALID_OPT > 0){
                VALID_OPT = OK;
                TURN = 0;
                game_board[65] = CPU_PIECE;
            }else{
                VALID_OPT = ILLMOVE;
            }
        }
    }
    up_write(&lock);
    return size;
}

int reversi_close(struct inode *inode, struct file *fptr){
    OPEN--;
    printk("reversi: closed device\n");
    module_put(THIS_MODULE);
    return 0;
}

/* Valid the user input */
static int input_val(const char *usrbuff){
    int valid = 0;
    if((strlen(usrbuff) > 7) && !(usrbuff[0] == 48)){
        return UNKCMD;
    }else if(!(usrbuff[0] == 48)){
        return UNKCMD;
    }
    if(strncmp(usrbuff, "00",2) == 0){
        if(strcmp(usrbuff, "00 X\n") == 0){
            valid = 1;
        }else if(strcmp(usrbuff, "00 O\n") == 0){
            valid = 2;
        }else{
            return INVFMT;
        }
        return valid;
    }else if(strncmp(usrbuff, "01", 2) == 0){
        if(strlen(usrbuff) > 3){
            return INVFMT;
        }
        return(valid = 3);
        
    }else if(usrbuff[0] == 48 && usrbuff[1] == 50){
        // if it is the players turn;
        if(RUNNING == 0){
            return NOGAME;
        }else if(strlen(usrbuff) > 7 || strlen(usrbuff) < 4) {
            return INVFMT;
        }
        if(isspace(usrbuff[2]) != 0){
            if(isdigit(usrbuff[3]) != 0){
                if((usrbuff[3] < 48) || (usrbuff[3] > 55)){                      //check if it between 0-7
                    return ILLMOVE;
                }
                if(isspace(usrbuff[4]) != 0){
                    if(isdigit(usrbuff[5]) != 0){
                        if((usrbuff[5] < 48) || (usrbuff[5] > 55)){              //check if it between 0-7
                            return ILLMOVE;
                        }
                        ROW = ((int)usrbuff[5]) - 48;
                        COLUMN = ((int)usrbuff[3])- 48;
                        return(valid = 4);
                    }
                }
            }
        }
        return INVFMT;

    }else if(strncmp(usrbuff, "03", 2) == 0){
        if(strlen(usrbuff) > 3){
            return INVFMT;
        }
        if(RUNNING == 0){
            return NOGAME;
        }
        return(valid = 5);
    }else if(strncmp(usrbuff, "04", 2) == 0){
        if(strlen(usrbuff) > 3){
            return INVFMT;
        }
        if(RUNNING == 0){
            return NOGAME;
        }
        return(valid = 6);
    }
    return UNKCMD;
}

/* initialize the board */
static void initial_board(){
    int i;
    for(i = 0; i < BOARD_LEN - 3;++i){
        game_board[i] = '-';
    }
    game_board[27] = 'O';
    game_board[28] = 'X';
    game_board[35] = 'X';
    game_board[36] = 'O';
    game_board[64] = '\t';
    game_board[65] = 'X';
    game_board[66] = '\n';
}

/* initialize a new board appropriate the user's piece of choice */
static void new_board(char piece, int firstTurn){
    RUNNING = 1;
    initial_board();
    if(piece == 'X'){
        PLAYER_PIECE = 'X';
        CPU_PIECE = 'O';
        TURN = firstTurn;
        PLAYER_SCORE = 2;
        CPU_SCORE = 2;        
    }else if(piece == 'O'){
        PLAYER_PIECE = 'O';
        CPU_PIECE = 'X';
        TURN = firstTurn;
        PLAYER_SCORE = 2;
        CPU_SCORE = 2;         
    }
}

/*if the x and y are within the bounds (0 && 7)*/
static int valid_cord(int x, int y){
    return((x >= MIN_ROW && x < MAX_ROWS) && (y >= MIN_ROW) && (y < MAX_ROWS));
}

/*returns if the position is between 0 && 67*/
static int valid_loc(int location){
    return((location >= 0) && (location <= 67));
}

/* Validates if the move from user/CPU is valid. If so update the board */
static int valid_Move(char piece, int row, int column){
    int flip_pieces;int i; int tmp; int curr_pos; int t_row; int t_col; 
    char oth_tile;
    curr_pos = (row*MAX_ROWS) + column;
    if(game_board[curr_pos] != '-'){                        /* Check if it is a valid space */
        return 0;
    }
    if(piece == 'X'){
        oth_tile = 'O';
    }else{
        oth_tile = 'X'; 
    }
    flip_pieces = 0;
    t_col = column;
    t_row = row; 
    for(i = 0; i < MAX_ROWS; i++){
        t_row = row + dir_arr[i][0];
        t_col = column + dir_arr[i][1];
        tmp = t_row *MAX_ROWS + t_col;
        /*make a function for checking t_row, t_col to be between 0 and 7*/
        if(valid_cord(t_row, t_col) && game_board[tmp] == oth_tile){
            /*traverse that direction until it is not the passed piece*/
            while(game_board[tmp] == oth_tile){
                t_row += dir_arr[i][0];
                t_col += dir_arr[i][1];
                tmp = t_row * MAX_ROWS + t_col;
                if(!valid_cord(t_row, t_col)){              /*make a function for checking t_row, t_col to be between 0 and 7*/
                    break;
                }
            }
            /*move temp location is on the board and the position is passed piece*/
            if(valid_cord(t_row, t_col) && game_board[tmp] == piece){
                /*get back to the original position of the piece*/
                while(tmp != curr_pos){
                    t_row -= dir_arr[i][0];
                    t_col -= dir_arr[i][1];
                    tmp = t_row * MAX_ROWS + t_col;
                    /*temp position is equivalent to the original position*/                    
                    if(tmp == curr_pos){
                        //game_board[tmp] = piece;
                        break;
                    /*flip the game piece for tiles that are opposite of piece*/
                    }else if(game_board[tmp] == oth_tile){
                        game_board[tmp] = piece;
                        flip_pieces++;
                    }
                } 
            }
        }
    }
    if(flip_pieces > 0){
        game_board[curr_pos] = piece;
    }     
    return flip_pieces;        
}

/* Check who the winner is */
static void win_check(){
    if(CPU_SCORE < PLAYER_SCORE && full_board() == 1){
        VALID_OPT = WIN;
    }else if(CPU_SCORE > PLAYER_SCORE && full_board() == 1){
        VALID_OPT = LOSE;
    }else if(CPU_SCORE == PLAYER_SCORE && full_board() == 1){
        VALID_OPT = TIE;
    }else if(pass_turn() == 1 && cpu_turn() == 0){
        if(CPU_SCORE < PLAYER_SCORE){
            VALID_OPT = WIN;
        }else if(CPU_SCORE > PLAYER_SCORE){
            VALID_OPT = LOSE;
        }else if(CPU_SCORE == PLAYER_SCORE){
            VALID_OPT = TIE;
        }
    }    
}

/*
Pass the user's turn because no valid moves exist. If a valid move exists for the player, 
responds with "ILLMOVE\n" and does not modify the game's state. 
Otherwise responds as does the 02 command above for the state of the game after the move.
*/
static int pass_turn(){
    int i; int j; int k; int row; int column; int pos; int can_pass;
    can_pass = 1;
    /*traverse to the number of rows*/
    for(i = 0; i < MAX_ROWS; i++){
        /*traverse to the number of columns*/
        for(j = 0; j < MAX_ROWS;j++){
            row = i;
            column = j;
            pos = (row * MAX_ROWS) + column;
            /* the position is on the board */
            if(valid_loc(pos)){
                /* the position is the player's piece */
                if(game_board[pos] == PLAYER_PIECE){
                    /*traverse to each direction of that piece to encapsulate if possible*/
                    for(k = 0; k < MAX_ROWS; k++){
                        row = i + dir_arr[k][0];
                        column = j + dir_arr[k][1];
                        pos = (row * MAX_ROWS) + column;
                        /*Check to see if the position is still on the board*/
                        if(valid_cord(row, column) && valid_loc(pos)){
                            if(game_board[pos] == CPU_PIECE){
                                /*traverse that direction*/
                                while(game_board[pos] == CPU_PIECE){
                                    row += dir_arr[k][0];
                                    column += dir_arr[k][1];
                                    pos = (row * MAX_ROWS) + column;
                                    /*make a function for checking t_row, t_col to be between 0 and 7*/
                                    if(!valid_cord(row, column)){
                                        break;
                                    }
                                }
                                if(valid_cord(row, column) && game_board[pos] == '-'){
                                    can_pass = ILLMOVE;
                                    i = MAX_ROWS;
                                    j = MAX_ROWS;
                                    k = MAX_ROWS;
                                }                               
                            }
                        }
                    }
                }
            } 
        }
    }
    return can_pass;
}

/*
Asks the computer to make a move. Responds with one of the responses documented above for the 02 command, as appropriate 
(however, the "ILLMOVE\n" error shall never be returned from this command — the computer shall never attempt to make an illegal move). 
This command takes no arguments. Responses such as "WIN\n" and "LOSE\n" still refer to the human player from this command, not to the CPU.
*/
static int cpu_turn(){
    int i; int j; int valid_turn;
    valid_turn = 0;
    for(i = 0;i < MAX_ROWS; i++){
        for (j= 0; j < MAX_ROWS; j++){
            valid_turn = valid_Move(CPU_PIECE, i, j);
            if(valid_turn > 0){
                i = MAX_ROWS;
                j = MAX_ROWS;
            }
        }
    }
    return valid_turn;
}

/* If the board has no more empty spaces */
static int full_board(){
    int i;
    for(i = 0; i <= 64;i++){
        if(game_board[i] == '-'){
            return 0;
        }
    } 
    return 1;
}

static int __init init_reversi(void){
    if(misc_register(&reversi_dev)){
        return -ENODEV;
    }
    if(RUNNING == 0){
        initial_board();
    }
    printk("init_reversi = true\n");
    return 0;
}

static void __exit exit_reversi(void){
    //kfree(output_str);
    misc_deregister(&reversi_dev);
    printk("exit_reversi = true\n");
}

module_init(init_reversi);
module_exit(exit_reversi);