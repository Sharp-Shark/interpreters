// interpreted interactive brainfuck extended

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

const char SAVEPATH[] = "save.txt";

const char X1B_RESET[] = "\x1b[0m";
const char X1B_UNDERLINE[] = "\x1b[4m";

char** functions;

int mod (int n, int k)
{
	return ((n % k) + k) % k;
}

void display (const unsigned int* tape, unsigned int size, unsigned int pos)
{
	printf("\nTape %d\n", size);
	for (int i = 0; i < size; i++)
	{
		if (i == pos)
		{
			fputs(X1B_UNDERLINE, stdout);
			printf("%u", tape[i]);
			fputs(X1B_RESET, stdout);
			putchar(' ');
		}
		else
		{
			printf("%u ", tape[i]);
		}
	}
	putchar('\n');
}

void registerFunction (char** functions, char key, const char* string)
{
	if (functions[key] != NULL) free(functions[key]);
	functions[key] = strdup(string);
}

void interpret (const char* string, unsigned int length, unsigned int* tape, unsigned int size, unsigned int* ppos)
{
	unsigned int pos = *ppos;
	int i = 0;
	int depth = 0;
	while (i >= 0 && i < length)
	{
		char c = string[i];
		if (depth == 0)
		{
			switch (c)
			{
				case '[' : if (tape[pos] == 0) depth += 1; break;
				case ']' : if (tape[pos] != 0) depth -= 1; break;
				case '+' : tape[pos] = mod(++tape[pos], size); break;
				case '-' : tape[pos] = mod(--tape[pos], size); break;
				case '>' : pos = mod(++pos, size); break;
				case '<' : pos = mod(--pos, size); break;
				case '.' : putchar(tape[pos]); break;
				// extended
				case '!' : display(tape, size, pos); break;
				case '&' : tape[pos] = pos; break;
				case '*' : pos = tape[pos]; break;
				case '@' : tape[pos] = tape[tape[pos]]; break;
				case '%' : tape[tape[pos]] = pos; break;
				case '0' :
				case '1' :
				case '2' :
				case '3' :
				case '4' :
				case '5' :
				case '6' :
				case '7' :
				case '8' :
				case '9' : tape[pos] = mod(c - '0' + tape[pos] * 10, size);
				default : if (functions[c] != NULL) { *ppos = pos; interpret(functions[c], strlen(functions[c]), tape, size, ppos); pos = *ppos; }
			}
		}
		else
		{
			switch (c)
			{
				case '[' : depth++; break;
				case ']' : depth--; break;
			}
		}
		i += depth >= 0 ? 1 : -1; 
	}
	*ppos = pos;
}

int save (const char* path, const unsigned int* tape, unsigned int size, unsigned int pos)
{
	FILE* file = fopen(path, "w");
	if (file == NULL) return 1;
	
	fputc('r', file);
	for (int i = 0; i < size; i++)
	{
		fprintf(file, "%u", tape[i]);
		fputc('>', file);
	}
	for (int i = 0; i < pos; i++) fputc('>', file);
	fclose(file);
	
	return 0;
}

int main ()
{
	unsigned int size = 256;
	unsigned int* tape = malloc(sizeof(int) * size); for (int i = 0; i < size; i++) tape[i] = 0;
	unsigned int pos = 0;
	
	unsigned int functionsSize = 256;
	functions = malloc(sizeof(char*) * functionsSize);  for (int i = 0; i < functionsSize; i++) functions[i] = NULL;
	// reset (pos, tape) -> (0, 0)
	registerFunction(functions, 'r', "[-]*[-] +[*[-]*+]");
	// add; (a, b) -> (a + b, 0)
	registerFunction(functions, 'a', "[-<+>]");
	// multiply; (a, b, c, d) -> (a+d + (b-1)(a+c+d), 0, 0, 0)
	registerFunction(functions, 'm', "<[->>>+<<<]> [- >>[-<+<<+>>>]< [->+<]<] >>[-]<<");
	// copy; (a, b, c) -> (a + c, a + b, 0)
	registerFunction(functions, 'c', "[->+>+<<] >> [-<<+>>] <<");
	// stack alloc; (pos, a, b) -> (pos + a + b + 1, pos + a + b + 1, 0)
	registerFunction(functions, 's', "+c[-]&>a<%*");
	// Hello, world.
	registerFunction(functions, 'h', "r72.[-]101.[-]108..>111.>44.[-]32.[-]119.[-]<.+++.<.[-]100.[-]46.r");
	
	unsigned int inputSize = 1024;
	char* input = malloc(sizeof(char) * inputSize);
	unsigned int inputLen = 0;
	
	system("color"); // for windows
	
	fputs(X1B_UNDERLINE, stdout);
	printf("Do help for a full list of commands.");
	fputs(X1B_RESET, stdout);
	fputs("\n", stdout);
	
	while (1)
	{
		display(tape, size, pos);
		putchar('\n');
		
		char* result = fgets(input, inputSize, stdin);
		if (result == NULL) break;
		inputLen = strlen(input);
		
		if (strncmp(input, "help", 4) == 0)
		{
			printf("help - Lists and explains commands.\n");
			printf("load {path=\"%s\"} - Executes a program from a file.\n", SAVEPATH);
			printf("save {path=\"%s\"} - Save a program that when executed restores the current tape.\n", SAVEPATH);
			printf("define {letter} {code} - Defines a function. Functions are not macros.\n");
			printf("resize {size} - Resizes tape to the specified size. Any current tape value x will become x modulo size.\n");
			printf("random {start=%d} {end=%d} - Randomizes cells in the range start..end.\n", 0, size - 1);
			printf("functions - Lists all defined functions.\n");
		}
		else if (strncmp(input, "load", 4) == 0)
		{
			char* path = NULL;
			int result = sscanf(input, "load %ms", &path);
			if (result < 1)
			{
				path = realloc(path, sizeof(char) * (strlen(SAVEPATH) + 1));
				strcpy(path, SAVEPATH);
			}
			
			FILE* file = fopen(path, "r");
			if (file != NULL)
			{
				char c;
				unsigned int newSize = 1; while ((c = fgetc(file)) != EOF) { newSize++; }
				if (newSize > inputSize)
				{
					inputSize = newSize;
					input = realloc(input, sizeof(char) * inputSize);
				}
				inputLen = newSize - 1;
				
				rewind(file);
				fread(input, inputSize, sizeof(char), file);
				fclose(file);
				interpret(input, inputLen, tape, size, &pos);
				
				printf("Loaded from \"%s\".\n", path);
			}
			
			free(path);
		}
		else if (strncmp(input, "save", 4) == 0)
		{
			char* path = NULL;
			int result = sscanf(input, "save %ms", &path);
			if (result < 1)
			{
				path = realloc(path, sizeof(char) * (strlen(SAVEPATH) + 1));
				strcpy(path, SAVEPATH);
			}
			
			result = save(path, tape, size, pos);
			if (result == 0) printf("Saved to \"%s\".\n", path);
			
			free(path);
		}
		else if (strncmp(input, "define", 6) == 0)
		{
			char letter;
			char* string = NULL;
			int result = sscanf(input, "define %c %m[^\n]", &letter, &string);
			
			if (result == 2)
			{
				if (0 <= letter && letter < functionsSize)
				{
					registerFunction(functions, letter, string);
					
					printf("Defined function '%c': \"%s\"\n", letter, string);
				}
				else
				{
					printf("'%c' is not a valid character.\n", letter);
				}
			}
			else
			{
				printf("Invalid definition.\n");
			}
			
			free(string);
		}
		else if (strncmp(input, "resize", 6) == 0)
		{
			unsigned int newSize = 0;
			int result = sscanf(input, "resize %u", &newSize);
			if (result == 1 && newSize > 0)
			{
				if (size == newSize)
				{
					printf("Tape is already at specified size.\n");
				}
				else
				{
					tape = realloc(tape, sizeof(int) * newSize);
					if (newSize > size)
					{
						for (int i = size; i < newSize; i++) tape[i] = 0;
					}
					else
					{
						for (int i = 0; i < newSize; i++) tape[i] = mod(tape[i], newSize);
					}
					size = newSize;
					
					printf("Resized tape to %u.\n", newSize);
				}
			}
			else
			{
				printf("Invalid size.\n");
			}
		}
		else if (strncmp(input, "random", 6) == 0)
		{
			unsigned int start, end;
			int result = sscanf(input, "random %u %u", &start, &end);
			if (result == 2)
			{
				start = start < 0 ? 0 : start;
				end = end >= size ? (size - 1) : end;
			}
			else if (result == 1)
			{
				start = start < 0 ? 0 : start;
				end = size - 1;
			}
			else
			{
				start = 0;
				end = size - 1;
			}
			
			srand(time(NULL));
			for (int i = start; i <= end; i++) tape[i] = mod(rand(), size);
			
			printf("Randomized range %u..%u.\n", start, end);
		}
		else if (strncmp(input, "functions", 7) == 0)
		{
			for (int i = 0; i < functionsSize; i++)
			{
				if (functions[i] != NULL) printf("'%c': \"%s\"\n", i, functions[i]);
			}
		}
		else
		{
			interpret(input, inputLen, tape, size, &pos);
		}
	}
	
	return 0;
}