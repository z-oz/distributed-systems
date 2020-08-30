/*-------------------------------------------------------------------------*
 *---									---*
 *---		symMathCalc.c						---*
 *---									---*
 *---	  A symbolic math calculator.					---*
 *---									---*
 *---	----	----	----	----	----	----	----	----	---*
 *---									---*
 *---	Version 1a		2020 April 7		Joseph Phillips	---*
 *---									---*
 *-------------------------------------------------------------------------*/

//
//	Compile with:
//	$ g++ symMathCalc.c -o symMathCalc -lncurses -g
//
//	NOTE:
//	If this program crashes then you may not see what you type.
//	If that happens, just type:
//
//		stty sane
//
//	to your terminal, even though you cannot even see it.

//---									---//
//---			Header file inclusions:				---//
//---									---//
#include	<stdlib.h>
#include	<stdio.h>
#include	<string.h>
#include	<unistd.h>	// For alarm()
#include	<errno.h>
#include	<sys/socket.h>
#include	<sys/types.h>	// For creat()
#include	<sys/wait.h>	// For creat(), wait()
#include	<sys/stat.h>	// For creat()
#include	<fcntl.h>	// For creat()
#include	<signal.h>
#include	<ncurses.h>


//---									---//
//---			Definitions of constants:			---//
//---									---//
#define		DEFAULT_CALC_PROGRAM	"/usr/bin/python3"
#define		PYTHON_NO_BUFFERING_ARG	"-u"
#define		PYTHON_PROGNAME		"symMath.py"
#define		PYTHON_QUIT_CMD		"quit()\n"
#define		PYTHON_PROMPT		">>> "

const int	HEIGHT			= 5;
const int	WIDTH			= 64;
const int	INIT_TEXT_LEN		= 1024;
const int	MAX_TEXT_LEN		= 65536;
const char	STOP_CHAR		= (char)27;
const char	CALC_CHAR		= (char)'\r';
const int	BUFFER_LEN		= 4096;
const int	TYPING_WINDOW_BAR_Y	= 0;
const int	CHECKING_WINDOW_BAR_Y	= TYPING_WINDOW_BAR_Y   + HEIGHT + 1;
const int	MESSAGE_WINDOW_BAR_Y	= CHECKING_WINDOW_BAR_Y + HEIGHT + 1;
const int	NUM_MESSAGE_SECS	= 4;


//---									---//
//---			Definitions of global vars:			---//
//---									---//
WINDOW*		typingWindow;
WINDOW*		answerWindow;
WINDOW*		messageWindow;
pid_t		childPid;
int		toPythonFd;
int		fromPythonFd;
int		shouldRun	= 1;
int		hasChildStopped	= 0;


//---									---//
//---			Definitions of global fncs:			---//
//---									---//

//  PURPOSE:  To turn 'ncurses' on.  No parameters.  No return value.
void		onNCurses	()
{
  //  I.  Application validity check:

  //  II.  Turn 'ncurses' on:
  const int	LINE_LEN	= 80;

  char		line[LINE_LEN];

  initscr();
  cbreak();
  noecho();
  nonl();
//intrflush(stdscr, FALSE);
//keypad(stdscr, TRUE);
  typingWindow		= newwin(HEIGHT,WIDTH,TYPING_WINDOW_BAR_Y+1,1);
  answerWindow		= newwin(HEIGHT,WIDTH,CHECKING_WINDOW_BAR_Y+1,1);
  messageWindow		= newwin(     1,WIDTH,MESSAGE_WINDOW_BAR_Y,1);
  scrollok(typingWindow,TRUE);
  scrollok(answerWindow,TRUE);

  snprintf(line,LINE_LEN,"'Esc' to quit. Enter to calculate");
  mvaddstr(TYPING_WINDOW_BAR_Y,0,line);
  mvaddstr(CHECKING_WINDOW_BAR_Y,0,"Answers:");
  refresh();
  wrefresh(typingWindow);	// moves cursor back to 'typingWindow':

  //  III.  Finished:
}


//  PURPOSE:  To handle the 'SIGCHLD' signal.  'sig' will be 'SIGCHLD' and
//	will be ignored.  No return value.
void		sigChildHandler	(int		sig
				)
{
  int	status;

  wait(&status);
  hasChildStopped	= 1;
}


//  PURPOSE:  To handle the 'SIGALRM' signal.  'sig' will be 'SIGALRM' and
//	will be ignored.  No return value.
void		sigAlarmHandler	(int		sig
				)
{
}


//  PURPOSE:  To launch the Python-implemented symbolic algebra calculator.
//	If 'argc' is at least 2 then 'argv[1]' will be run.
//	If 'argc' is less than 2 then 'DEFAULT_CALC_PROGRAM' will be run.
//	Sets 'childPid' to the process id of 'childPid'.  No return value.
void		launchSymPy	(int		argc,
				 char*		argv[]
				)
{
  //  I.  Application validity check:

  //  II.  Attempt to launch Python-implemented symbolic algebra calculator:
  int		toPythonArray[2];
  int		fromPythonArray[2];
  const char*	path	= (argc > 1) ? argv[1] : DEFAULT_CALC_PROGRAM;

  if  ( (pipe(toPythonArray) < 0) || (pipe(fromPythonArray) < 0) )
  {
    fprintf(stderr,"pipe() failed: %s\n",strerror(errno));
    exit(EXIT_FAILURE);
  }

  childPid		= fork();

  if  (childPid < 0)
  {
    fprintf(stderr,"fork() failed: %s\n",strerror(errno));
    exit(EXIT_FAILURE);
  }

  if  (childPid == 0)
  {
    close(toPythonArray[1]);
    dup2(toPythonArray[0],STDIN_FILENO);
    close(fromPythonArray[0]);
    dup2(fromPythonArray[1],STDOUT_FILENO);
    execl(path,path,PYTHON_NO_BUFFERING_ARG,PYTHON_PROGNAME,NULL);
    fprintf(stderr,"Cannot run \"%s\".\n",path);
    exit(EXIT_FAILURE);
  }

  toPythonFd	= toPythonArray[1];
  close(toPythonArray[0]);
  fromPythonFd	= fromPythonArray[0];
  close(fromPythonArray[1]);

  //  III.  Finished: 
}


//  PURPOSE:  To save the 'lineIndex' chars at the beginning of 'line' to
//	to position '*endTextPtrPtr' in buffer '*bufferPtrPtr' of length
//	'*bufferLenPtr' and with end '*endBufferPtrPtr'.  If there is not
//	enough space in '*bufferPtrPtr' then '*bufferLenPtr' will be doubled,
//	and '*bufferPtrPtr' will be 'realloc()'-ed to this new length.
//	No return value.
void		saveLine	(size_t*	bufferLenPtr,
				 char**		bufferPtrPtr,
				 char**		endTextPtrPtr,
				 char**		endBufferPtrPtr,
				 const char*	line,
				 int   		lineIndex
				)
{
  //  I.  Application validity check:

  //  II.  Save 'line' to '*bufferPtrPtr':
  //  II.A.  Allocate more space if needed:
  if  (lineIndex >= (*endBufferPtrPtr - *endTextPtrPtr + 1) )
  {
    size_t	textLen	 = *endTextPtrPtr - *bufferPtrPtr;

    (*bufferLenPtr)	*= 2;
    (*bufferPtrPtr)	 = (char*)realloc(*bufferPtrPtr,*bufferLenPtr);
    (*endTextPtrPtr)	 = *bufferPtrPtr + textLen;
    (*endBufferPtrPtr)	 = *bufferPtrPtr + *bufferLenPtr;
  }

  //  II.B.  Save 'line' to '*bufferPtrPtr':
  memcpy(*endTextPtrPtr,line,lineIndex);
  (*endTextPtrPtr)	+= lineIndex;

  //  III.  Finished:
}



//  PURPOSE:  To wait for Python to respond to the most recent command sent
//	to it.  We know Python has completed its response after we receive
//	'PYTHON_PROMPT'.  'fromPythonFd' is the file descriptor for
//	listening to Python.  Returns response from Python, without the
//	'PYTHON_PROMPT'.
int		waitForPrompt	(int		fromPythonFd,
				 char*		buffer,
				 int		bufferLen
				)
{
  char*		promptPtr;
  int		numBytes;
  int		totalBytes	= 0;

  while  ( (numBytes =
		read(fromPythonFd,buffer+totalBytes,bufferLen-1-totalBytes)
	   )
	   > 0
	 )
  {
    promptPtr	= strstr(buffer+totalBytes,PYTHON_PROMPT);

    if  (promptPtr != NULL)
    {
      *promptPtr	= '\0';
      return(promptPtr - buffer);
    }

    totalBytes	+= numBytes;
  }

  return(numBytes);
}


//  PURPOSE:  To attempt to write the text pointed to by 'bufferPtr' to a PDF
//	file named 'PDF_FILENAME'.  'endTextPtr' points to one char beyond
//	the end of the text to print in 'bufferPtr'.  No return value.
//
//	SIDE EFFECT: Prints to 'answerWindow'
void		calculate	(const char*	outputBufferPtr,
				 const char*	endTextPtr
				)
{
  //  I.  Application validity check:

  //  II.  Send 'outputBufferPtr' to calculator:
  char	inputBuffer[BUFFER_LEN];
  int	numBytes;

  write(toPythonFd,outputBufferPtr,endTextPtr-outputBufferPtr);
  alarm(1);
  numBytes	= waitForPrompt(fromPythonFd,inputBuffer,BUFFER_LEN);
  alarm(0);

  if  (numBytes > 0)
  {
    if  (endTextPtr[-1] == '\r'  ||  endTextPtr[-1] == '\n')
    {
      endTextPtr--;
    }

    waddnstr(answerWindow,outputBufferPtr,endTextPtr-outputBufferPtr);
    waddstr (answerWindow," = ");
    waddstr(answerWindow,inputBuffer);
    wrefresh(answerWindow);
    wrefresh(typingWindow);	// moves cursor back to 'typingWindow':
  }

  //  III.  Finished:
}


//  PURPOSE:  To allow the user to type, display what they type in
//	'typingWindow', and to send what they type to the spell checking
//	process upon pressing 'Enter'.  'vPtr' comes in, perhaps pointing
//	to something.  Returns 'NULL'.
void*		type		(void*	vPtr
				)
{
  //  I.  Application validity check:

  //  II.  Handle user typing:
  unsigned int	c;
  char		line[WIDTH+1];
  int		index		= 0;
  size_t	bufferLen	= INIT_TEXT_LEN;
  char*		bufferPtr	= (char*)malloc(bufferLen);
  char*		endTextPtr	= bufferPtr;
  char*		endBufferPtr	= bufferPtr + bufferLen;

  //  II.A.  Each iteration handles another typed char:
  while  ( (c = getch()) != STOP_CHAR )
  {

    //  II.A.1.  Handle special chars:
    if  (c == '\r')
    {
      //  II.A.1.a.  Treat carriage return like newline:
      c = '\n';
    }
    else
    if  ( (c == 0x7) || (c == 127) )
    {
      //  II.A.1.b.  Handle backspace:
      int	col	= getcurx(typingWindow);

      if  (col > 0)
      {
        index--;
        wmove(typingWindow,getcury(typingWindow),col-1);
	wrefresh(typingWindow);
      }

      continue;
    }
    else
    if  (c == ERR)
    {
      continue;
    }

    //  II.A.2.  Print and record the char:
    waddch(typingWindow,c);
    wrefresh(typingWindow);
    line[index++]	= c;

    //  II.A.3.  Handle when save 'line':
    if  (c == '\n')
    {
      //  II.A.3.a.  Save 'line' when user types newline:
      size_t	textLen	= endTextPtr - bufferPtr;

      saveLine(&bufferLen,&bufferPtr,&endTextPtr,&endBufferPtr,line,index);
      index = 0;
      calculate(bufferPtr,endTextPtr);
      endTextPtr	= bufferPtr + textLen;
      continue;
    }
    else
    if  (index == WIDTH-1)
    {
      //  II.A.3.b.  Save 'line' when at last column:
      line[index] = '\n';
      index++;
      saveLine(&bufferLen,&bufferPtr,&endTextPtr,&endBufferPtr,line,index);
      index = 0;
      waddch(typingWindow,'\n');
      wrefresh(typingWindow);
    }
  }

  //  III.  Finished:
  saveLine(&bufferLen,&bufferPtr,&endTextPtr,&endBufferPtr,line,index);
  write(toPythonFd,PYTHON_QUIT_CMD,sizeof(PYTHON_QUIT_CMD)-1);
  free(bufferPtr);
  return(NULL);
}


//  PURPOSE:  To turn off 'ncurses'.  No parameters.  No return value.
void		offNCurses	()
{
  sleep(1);
  nl();
  echo();
  refresh();
  delwin(messageWindow);
  delwin(typingWindow);
  delwin(answerWindow);
  endwin();
}



//  PURPOSE:  To do the spell-checking word-processor.  Ignores command line
//	arguments.  Return 'EXIT_SUCCESS' to OS.
int		main		(int		argc,
				 char*		argv[]
				)
{
  struct sigaction	act;
  char			buffer[BUFFER_LEN];

  memset(&act,'\0',sizeof(act));
  act.sa_handler	= sigChildHandler;
  sigaction(SIGCHLD,&act,NULL);
  act.sa_handler	= sigAlarmHandler;
  sigaction(SIGALRM,&act,NULL);

  onNCurses();
  launchSymPy(argc,argv);
  waitForPrompt(fromPythonFd,buffer,BUFFER_LEN);
  type(NULL);
  offNCurses();

  while  (!hasChildStopped)
  {
    sleep(1);
  }

  return(EXIT_SUCCESS);
}
