/* TODO:
 * - Somehow enable printing 0.
 * - Handle resize signal.
 * - Proper documentation of functions.
 */

#include <ncurses.h> /* tui interface */
#include <stdlib.h>
#include <signal.h>   /* sigaction() */
#include <unistd.h>   /* alarm() */
#include <string.h>   /* memset() */

/*
 ===============================================================================
 |                                    Data                                     |
 ===============================================================================
 */
/* = SIGNALS = */
static volatile sig_atomic_t is_interrupted = 0;

/* = NCURSES = */
static WINDOW *mainwin;
static int     term_row, term_col; /* info about the terminal screen */

/* output string */
#define STRING_MAX 4096 /* Max string length */
static char         output_string[STRING_MAX] = { 0 };
static unsigned int output_string_len         = 0;
static unsigned int output_string_i           = 0;

static int is_capital = 0, is_waiting = 0;

/*
 ===============================================================================
 |                            Function Declarations                            |
 ===============================================================================
 */
/* = STATUS = */
static void
status_print(const char *fmt, ...);
/* Sets the text on status and refreshes the screen too. */

/* = CASE STATUS = */
static void
case_status_print(int is_capital);
/*
 * Prints information about the case sensitivity on the status.
 *
 * Prints "Capital letters" when `is_capital` is 1 else "Small letters".
 */

/* = OUTPUT CHAR = */
static void
key_index_incr_wrap(int input_ch, int *key_index);
/*
 * Increases the key index value for the input along with proper wrapping of the
 * index.
 *
 * https://i.gadgets360cdn.com/products/large/1517304275_635_nokia_3310_4g.jpg
 */

static char
get_output_ch(char input_ch, int key_index);
/* Returns the correct output character with the given input character and the
 * key index. */

/* = OUTPUT STRING = */
static void
output_string_update(void);
/* Update the display of output string on the screen. */

static void
output_string_print(const char *fmt, ...);
/* Sets the output string to the given string. */

static void
output_string_ch_remove(void);
/* Remove the character in the current cursor position of output string. */

static void
output_string_clear(void);
/* Clears the output string. */

static void
toggle_case(void);
/* Toggle case. */

static void
output_string_copy_to_cb(void);
/*
 * Copies the output string to the clipboard.
 *
 * Internally runs xclip command and thus it has to be installed on the system.
 */

/* = OUTPUT STRING CURSOR = */
static void
output_string_cursor_back(void);
/* Moves the cursor one step backwards. */

static void
output_string_cursor_frwd(void);
/* Moves the cursor one step forwards. */

static void
output_string_cursor_update(void);
/* Update the terminal cursor to the current cursor of output string. */

static void
output_string_ch_insert(char ch);
/* Insert a character in the current cursor position of output string. */

static void
waiting_off(void);
/* Turns off the waiting signal. */

static void
waiting_on(void);
/* Turns on the waiting signal. */

/* = STRING MANIPULATION = */
static void
str_ch_insert(char *str, unsigned int *str_len, unsigned int index, char ch);
/* Inserts the given chracter in the given index. */

static void
str_ch_remove(char *str, unsigned int *str_len, unsigned int index);
/* Removes the character in the given index of the string. */

/* = SIGNAL = */
static void
set_signals(void);
/* Sets necessary signals. */

static void
signal_handler(int signum);
/* Handler function to run when a signal is received by the program. */

/* = CORE = */
static void
init_ncurses(void);
/* Initializes ncurses interface. */

static void
clean_and_exit(void);
/* Properly cleans/disables the ncurses interface and exits the program. */

/*
 ===============================================================================
 |                          Function Implementations                           |
 ===============================================================================
 */
/* = STATUS = */
static void
status_print(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	/* get current cursor position */
	int cur_y, cur_x;
	getyx(stdscr, cur_y, cur_x);

	/* move to the row of status, clear the row and print */
	move(term_row - 2, 0);
	clrtoeol();
	vw_printw(stdscr, fmt, args);

	/* move back to the previous position */
	move(cur_y, cur_x);

	va_end(args);
	refresh();
}

/* = CASE STATUS = */
static void
case_status_print(int is_capital)
{
	/* get current cursor position */
	int cur_y, cur_x;
	getyx(stdscr, cur_y, cur_x);

	/* move to the row of case status and clear the row */
	move(term_row - 3, 0);
	clrtoeol();

	/* print the case */
	if (is_capital)
		printw("Capital letters");
	else
		printw("Small letters");

	/* move back to the previous position */
	move(cur_y, cur_x);

	refresh();
}

/* = OUTPUT CHAR = */
static void
key_index_incr_wrap(int input_ch, int *key_index)
{
	int index = *key_index;

	if (input_ch == '7' || input_ch == '9') {
		if (index == 4) {
			*key_index = 0;
			return;
		}
	} else {
		if (index == 3) {
			*key_index = 0;
			return;
		}
	}

	*key_index = ++index;
}

static char
get_output_ch(char input_ch, int key_index)
{
	/* TODO: Document about the last element '[' */
	int keys_lut[9] = {
		'A', 'D', 'G', 'J', 'M', 'P', 'T', 'W', '['
	}; /* starting ascii decimal number of all keypad alphabets */

	int output_key = keys_lut[input_ch - '2'] + key_index;

	if (output_key == keys_lut[input_ch - '2' + 1])
		return input_ch;

	if (!is_capital)
		output_key += 'a' - 'A';
	return output_key;
}

/* = OUTPUT STRING = */
static void
output_string_update(void)
{
	output_string_print(output_string);
}

static void
output_string_print(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	/* get current cursor position */
	int cur_y, cur_x;
	getyx(stdscr, cur_y, cur_x);

	/* move to status, clear the row and print */
	move(0, 0);
	clrtoeol();
	vw_printw(stdscr, fmt, args);

	/* move back to the previous position */
	move(cur_y, cur_x);

	va_end(args);

	/* update the cursor */
	output_string_cursor_update();

	refresh();
}

static void
output_string_ch_insert(char ch)
{
	str_ch_insert(output_string, &output_string_len, output_string_i, ch);
	output_string_update();
	output_string_cursor_update();
}

static void
output_string_ch_remove(void)
{
	str_ch_remove(output_string, &output_string_len, output_string_i);
	output_string_update();
	output_string_cursor_update();
}

static void
output_string_clear(void)
{
	memset(output_string, 0, STRING_MAX);
	output_string_update();

	output_string_len = 0;

	output_string_i = 0;
	output_string_cursor_update();

	is_waiting = 0;
}

static void
toggle_case(void)
{
	is_capital = !is_capital;
	case_status_print(is_capital);
}

static void
output_string_copy_to_cb(void)
{
	char cmd[STRING_MAX + 50];

	strcpy(cmd, "echo -n \"");
	strncat(cmd, output_string, output_string_len);
	strcat(cmd, "\" | xclip -selection clipboard");

	system(cmd);
}

/* = OUTPUT STRING CURSOR = */
static void
output_string_cursor_back(void)
{
	if (output_string_i > 0)
		output_string_i--;
}

static void
output_string_cursor_frwd(void)
{
	if (output_string_i != STRING_MAX - 1)
		output_string_i++;
}

static void
output_string_cursor_update(void)
{
	move(0, output_string_i);
}

static void
waiting_off(void)
{
	is_waiting = 0;
	alarm(0);
}

static void
waiting_on(void)
{
	is_waiting = 1;
	alarm(1);
}

/* = STRING MANIPULATION = */
static void
str_ch_insert(char *str, unsigned int *str_len, unsigned int index, char ch)
{
	unsigned int len = *str_len;

	if (index > len)
		index = len;

	for (unsigned int i = len; i > index; i--) {
		str[i] = str[i - 1];
	}
	str[index] = ch;

	*str_len = len + 1;

	output_string_i = index;
}

static void
str_ch_remove(char *str, unsigned int *str_len, unsigned int index)
{
	unsigned int len = *str_len;

	if (index > len)
		index = len - 1;

	for (unsigned int i = index; i < len - 1; i++) {
		str[i] = str[i + 1];
	}

	str[len - 1] = '\0';
	*str_len     = len - 1;

	output_string_i = index;
}

/* = SIGNAL = */
static void
set_signals(void)
{
	struct sigaction sa;

	/* fill the sigaction struct */
	sa.sa_handler = signal_handler;
	sa.sa_flags   = 0;
	sigemptyset(&sa.sa_mask);

	/*  set signal handlers  */
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGALRM, &sa, NULL);
}

static void
signal_handler(int signum)
{
	is_interrupted = 1;

	/*  switch on signal number  */
	switch (signum) {
	case SIGALRM:
		output_string_cursor_frwd();
		output_string_cursor_update();
		is_waiting = 0;
		status_print("");
		return;
	case SIGINT:
	case SIGTERM:
		/*  clean up nicely  */
		clean_and_exit();
	}
}

/* = CORE = */
static void
init_ncurses(void)
{
	if ((mainwin = initscr()) == NULL) {
		perror("error initialising ncurses");
		exit(EXIT_FAILURE);
	}

	noecho();
	keypad(mainwin, TRUE);

	getmaxyx(mainwin, term_row, term_col);
}

static void
clean_and_exit(void)
{
	delwin(mainwin);
	endwin();
	refresh();
	exit(EXIT_SUCCESS);
}

int
main(void)
{
	set_signals();
	init_ncurses();

	/* initial screen output */
	case_status_print(is_capital);
	status_print(
		"Press one of the followings: 0-9, *, #, backspace, c, y, q");
	output_string_print(output_string);
	output_string_cursor_update();

	/* = MAIN LOOP = */
	int key_index = -1, last_input_ch = 0;
	while (1) {
		int input_ch = getch();
		if (input_ch == ERR) {
			/* BUG: After a real interrupt, this won't be able to
			 * catch real ERR on getch() */
			if (is_interrupted) /* getch() returns ERR on an interrupt */
				continue;
			perror("Cannot get the input!");
			clean_and_exit();
		}

		/* first check if the input is a special characters */
		int is_input_special_char = 0;
		switch (input_ch) {
		case '1':
			if (is_waiting)
				output_string_cursor_frwd();
			output_string_ch_insert('1');
			output_string_cursor_frwd();
			is_input_special_char = 1;

			status_print("1");
			break;
		case '0': /* user wants to add a space */
			if (is_waiting)
				output_string_cursor_frwd();
			output_string_ch_insert(' ');
			output_string_cursor_frwd();
			is_input_special_char = 1;

			status_print("0 - Space");
			break;
		case 263: /* user wants to remove a character */
			if (output_string_len == 0)
				continue;
			if (!is_waiting)
				output_string_cursor_back();
			output_string_ch_remove();
			is_input_special_char = 1;

			status_print("Backspace");
			break;
		case 'c': /* user wants to clear */
			output_string_clear();
			is_input_special_char = 1;

			status_print("c - Clear output");
			break;
		case 'y': /* user wants to yank the output string to clipboard */
			output_string_copy_to_cb();
			output_string_cursor_frwd();
			is_input_special_char = 1;

			status_print("y - Yank output string to clipboard");
			break;
		case '#': /* user wants to toggle case */
			toggle_case();
			output_string_cursor_frwd();
			is_input_special_char = 1;

			status_print("# - Toggle case");
			break;
		case 'q': /* user wants to quit */
			clean_and_exit();
		}
		/* disable waiting and continue the loop on special characters */
		if (is_input_special_char) {
			waiting_off();
			output_string_cursor_update();
			continue;
		}

		/* check if the input is valid */
		/* NOTE: Starting from 2 */
		if (input_ch < '2' || input_ch > '9') {
			status_print("Invalid input.");
			continue;
		}

		/* At this point we have a valid 2-9 input */
		if (is_waiting && input_ch == last_input_ch) {
			key_index_incr_wrap(input_ch, &key_index);
		} else {
			key_index  = 0;
			is_waiting = 0;
		}
		last_input_ch = input_ch;

		char output_ch = get_output_ch(input_ch, key_index);
		if (is_waiting) {
			output_string_ch_remove();
		} else {
			output_string_cursor_frwd();
		}
		output_string_ch_insert(output_ch);
		waiting_on();

		/* print currently typed char on status */
		status_print("%c", input_ch);
	}
}
