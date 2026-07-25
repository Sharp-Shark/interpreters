#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define BLANKSIZE 8
#define MAXPROGRAMSIZE 2048
#define LANGSIZE 38

const char SAVEPATH[] = "save.csv";

const char X1B_RESET[] = "\x1b[0m";
const char X1B_UNDERLINE[] = "\x1b[4m";

const char INVALIDCHAR = '?';
const char EMPTYCHAR = '_';

// global mutable
bool NEW = true;
bool OLD = false;
bool IGNOREINVALID = true;

char map (char c)
{
	if ('0' <= c && c <= '9') return c - '0' + 1; // 0..9 -> 1..10
	if ('a' <= c && c <= 'z') return c - 'a' + 11 ; // a..z -> 11..36
	if (c == EMPTYCHAR) return 37;
	return 0;
}

char unmap (char c)
{
	if (1 <= c && c <= 10) return c + '0' - 1;
	if (11 <= c && c <= 36) return c + 'a' - 11;
	if (c == 37) return EMPTYCHAR;
	return INVALIDCHAR;
}

void help ()
{
	printf("Do %shelp%s for a full list of commands.\n", X1B_UNDERLINE, X1B_RESET);
}

struct State
{
	unsigned int* indexes[LANGSIZE];
	unsigned int length[LANGSIZE];
	unsigned int size[LANGSIZE];
	unsigned int instances;
	unsigned int buffered; // instances buffered
};

void State_init (struct State* state)
{
	for (char c = 0; c < LANGSIZE; c++)
	{
		state->indexes[c] = NULL;
		state->length[c] = 0;
		state->size[c] = 0;
	}
	state->instances = 0;
}

void State_empty (struct State* state, char c)
{
	free(state->indexes[c]);
	state->indexes[c] = NULL;
	state->length[c] = 0;
	state->size[c] = 0;
	state->instances = 0;
}

void State_free (struct State* state)
{
	for (char c = 0; c < LANGSIZE; c++) State_empty(state, c);
}

bool State_remove (struct State* state, char c, unsigned int index)
{
	c = map(c);
	
	bool found = false;
	for (unsigned int i = 0; i < state->length[c]; i++)
	{
		if (state->indexes[c][i] == index)
		{
			index = i;
			found = true;
			break;
		}
	}
	if (!found) return false;
	
	unsigned int last = state->length[c] - 1;
	unsigned int copy = state->indexes[c][last];
	state->indexes[c][last] = state->indexes[c][index];
	state->indexes[c][index] = copy;
	
	state->length[c] -= 1;
	
	return true;
}

void State_append (struct State* state, char c, unsigned int index)
{
	c = map(c);
	
	if (state->length[c] >= state->size[c])
	{
		unsigned int newSize = state->size[c] > 0 ? (state->size[c] * 2) : 1;
		
		state->indexes[c] = realloc(state->indexes[c], sizeof(unsigned int) * newSize);
		state->size[c] = newSize;
	}
	state->indexes[c][state->length[c]] = index;
	state->length[c] += 1;
}

void State_copy (struct State* state, char c, unsigned int* indexes, unsigned int length)
{
	c = map(c);
	
	State_empty(state, c);
	
	state->indexes[c] = malloc(sizeof(unsigned int) * length);
	for (unsigned int i = 0; i < length; i++) state->indexes[c][i] = indexes[i];
	state->length[c] = length;
	state->size[c] = length;
}

void State_move (struct State* state, char c, unsigned int* indexes, unsigned int length)
{
	c = map(c);
	
	State_empty(state, c);
	
	state->indexes[c] = indexes;
	state->length[c] = length;
	state->size[c] = length;
}

struct Program
{
	struct State* states;
	unsigned int length;
	unsigned int size;
};

void Program_init (struct Program* program)
{
	program->states = NULL;
	program->length = 0;
	program->size = 0;
}

void Program_free (struct Program* program)
{
	if (program->states != NULL) for (unsigned int i = 0; i < program->length; i++) State_free(&program->states[i]);
	free(program->states);
	program->states = NULL;
	program->length = 0;
	program->size = 0;
}

void Program_resize (struct Program* program, unsigned int size)
{
	program->states = realloc(program->states, sizeof(struct State) * size);
	program->size = size;
}

void Program_contract (struct Program* program)
{
	if (program->length <= 0) return;
	
	State_free(&program->states[program->length - 1]);
	program->length -= 1;
}

void Program_expand (struct Program* program)
{
	if (program->length >= program->size) Program_resize(program, program->size > 0 ? (program->size * 2) : 1);
	
	State_init(&program->states[program->length]);
	program->length += 1;
}

void Program_loadCSV (struct Program* program, FILE* file)
{
	unsigned int column = 0;
	unsigned int row = 0;
	
	unsigned int columnCharSize = 64;
	unsigned int columnCharLength = 0;
	char* columnChar = malloc(sizeof(char) * columnCharSize);
	
	unsigned int buildSize = 1024;
	unsigned int buildLength = 0;
	char* build = malloc(sizeof(char) * buildSize);
	
	Program_free(program);
	
	bool newColumn = true;
	char c;
	while (c = fgetc(file))
	{
		if (buildLength >= buildSize)
		{
			buildSize = buildSize * 2;
			build = realloc(build, sizeof(char) * buildSize);
		}
		build[buildLength] = c;
		buildLength += 1;
		
		if (c == '\r') buildLength -= 1;
		if (c == ',' || c == ';' || c == '\n' || c == EOF)
		{
			if (buildLength > 1)
			{
				build[buildLength - 1] = '\0';
				
				if (row == 0)
				{
					if (columnCharLength >= columnCharSize)
					{
						columnCharSize = columnCharSize * 2;
						columnChar = realloc(columnChar, sizeof(unsigned int) * columnCharSize);
					}
					
					columnChar[column] = map(build[0]);
					columnCharLength += 1;
				}
				else
				{
					if (newColumn)
					{
						Program_expand(program);
						newColumn = false;
					}
					
					char a = columnChar[column];
					if (column >= columnCharLength) a = 0;
					State_append(&program->states[program->length - 1], unmap(a), atoi(build));
				}
			}
			buildLength = 0;
			if (c == ';') column += 1;
			if (c == '\n')
			{
				column = 0;
				row += 1;
				newColumn = true;
			}
		}
		
		if (c == EOF) break;
	}
	
	free(columnChar);
	free(build);
}

void Program_saveCSV (struct Program* program, FILE* file)
{
	unsigned char c = 0;
	for (char c = 0; c < LANGSIZE - 1; c++) fprintf(file, "%c;", unmap(c));
	fputc(unmap(LANGSIZE - 1), file);
	
	for (unsigned int i = 0; i < program->length; i++)
	{
		fputc('\n', file);
		for (char c = 0; c < LANGSIZE; c++)
		{
			for (unsigned int j = 0; j < program->states[i].length[c]; j++)
			{
				fprintf(file, "%d,", program->states[i].indexes[c][j]);
			}
			fputc(';', file);
		}
	}
}

void program_loadDefault (struct Program* program)
{
	Program_free(program);
	
	Program_expand(program);
	State_copy(&program->states[0], EMPTYCHAR, (unsigned int[]) {0}, 1);
	State_copy(&program->states[0], 'a', (unsigned int[]) {2}, 1);
	State_copy(&program->states[0], 'd', (unsigned int[]) {1}, 1);
	
	Program_expand(program);
	State_copy(&program->states[1], EMPTYCHAR, (unsigned int[]) {1}, 1);
	State_copy(&program->states[1], 'a', (unsigned int[]) {0}, 1);
	State_copy(&program->states[1], 's', (unsigned int[]) {1}, 1);
	State_copy(&program->states[1], 'd', (unsigned int[]) {2}, 1);
	State_copy(&program->states[1], 'f', (unsigned int[]) {1, 2}, 2);
	
	Program_expand(program);
	State_copy(&program->states[2], EMPTYCHAR, (unsigned int[]) {2}, 1);
	State_copy(&program->states[2], 'a', (unsigned int[]) {1}, 1);
	State_copy(&program->states[2], 's', (unsigned int[]) {2}, 1);
	State_copy(&program->states[2], 'd', (unsigned int[]) {0}, 1);
	State_copy(&program->states[2], 'f', (unsigned int[]) {2}, 1);
}

struct Machine
{
	struct Program* program;
	unsigned int* indexes;
	unsigned int length;
	unsigned int size;
};

void Machine_init (struct Machine* machine)
{
	machine->program = NULL;
	machine->indexes = NULL;
	machine->length = 0;
	machine->size = 0;
}

void Machine_free (struct Machine* machine)
{
	if (machine->program != NULL) Program_free(machine->program);
	machine->program = NULL;
	
	free(machine->indexes);
	machine->indexes = NULL;
	machine->length = 0;
	machine->size = 0;
}

void Machine_setup (struct Machine* machine)
{
	Machine_free(machine);
	
	machine->program = malloc(sizeof(struct Program));
	Program_init(machine->program);
}

void Machine_resize (struct Machine* machine, unsigned int size)
{
	machine->indexes = realloc(machine->indexes, sizeof(unsigned int) * size);
	machine->size = size;
}

void Machine_start (struct Machine* machine, unsigned int index)
{
	if (machine->length >= machine->size) Machine_resize(machine, machine->size > 0 ? (machine->size * 2) : 1);
	
	machine->indexes[machine->length] = index;
	machine->length += 1;
}

void Machine_halt (struct Machine* machine, unsigned int index)
{
	if (index < 0 || index >= machine->length) return;
	unsigned int last = machine->length - 1;
	
	unsigned int copy = machine->indexes[last];
	machine->indexes[last] = machine->indexes[index];
	machine->indexes[index] = copy;
	
	machine->length -= 1;
}

void Machine_kill (struct Machine* machine)
{
	free(machine->indexes);
	machine->indexes = NULL;
	machine->length = 0;
	machine->size = 0;
	
	if (machine->program != NULL)
	{
		for (unsigned int i = 0; i < machine->program->length; i++)
		{
			machine->program->states[i].instances = 0;
		}
	}
}

void Machine_render (struct Machine* machine)
{
	struct Program* program = machine->program;
	if (program->length == 0)
	{
		printf("Program is empty.\n");
		printf("Do %sload%s to load a program from \"%s\".\n", X1B_UNDERLINE, X1B_RESET, SAVEPATH);
		printf("Do %sblank%s to create a blank program of size %d.\n", X1B_UNDERLINE, X1B_RESET, BLANKSIZE);
		help();
		return;
	}
	
	unsigned int instances = machine->length * OLD;
	if (NEW) for (unsigned int i = 0; i < program->length; i++) instances += program->states[i].instances;
	printf("Active branches: %d\n", instances);
	
	unsigned int* array = malloc(sizeof(unsigned int) * program->length);
	for (unsigned int i = 0; i < program->length; i++) array[i] = 0;
	for (unsigned int i = 0; i < machine->length; i++) array[machine->indexes[i]] += 1;
	
	for (unsigned int i = 0; i < program->length; i++)
	{
		struct State* state = &program->states[i];
		
		printf("State %d", i);
		unsigned int sum = NEW * state->instances + OLD * array[i];
		if (sum == 1) printf(" (%d branch)", sum);
		else if (sum > 1) printf(" (%d branch)", sum);
		if (OLD && NEW && state->instances + array[i] > 0) printf(" (b:%d; s:%d)", state->instances, array[i]);
		putchar('\n');
		
		bool empty = true;
		for (char c = 0; c < LANGSIZE; c++)
		{
			unsigned int length = state->length[c];
			if (length > 0)
			{
				empty = false;
				printf("	%c: ", unmap(c));
				for (unsigned int j = 0; j < length; j++)
				{
					printf("%d", state->indexes[c][j]);
					if (j < length - 1) printf(", ");
				}
				putchar('\n');
			}
		}
		if (empty) printf("	HALT\n");
	}
	
	free(array);
}

void Machine_tick (struct Machine* machine, char cin)
{
	switch (cin)
	{
		case '!' :
		{
			Machine_render(machine);
			break;
		}
		default :
		{
			char c = map(cin);
			if (c == 0 && cin != INVALIDCHAR && IGNOREINVALID) return;
			printf("Received '%c'\n", unmap(c));
			
			struct Program* program = machine->program;
			if (program == NULL) return;
			
			if (NEW)
			{
				for (unsigned int i = 0; i < program->length; i++) program->states[i].buffered = 0;
				for (unsigned int i = 0; i < program->length; i++)
				{
					struct State* state = &program->states[i];
					
					unsigned int instances = state->instances;
					if (instances > 0)
					{
						for (unsigned int j = 0; j < state->length[c]; j++)
						{
							unsigned int index = state->indexes[c][j];
							if (index < program->length) program->states[index].buffered += instances;
						}
					}
				}
				for (unsigned int i = 0; i < program->length; i++) program->states[i].instances = program->states[i].buffered;
			}
			
			if (OLD)
			{
				unsigned int appendSize = 16;
				unsigned int appendLength = 0;
				unsigned int* append = malloc(sizeof(unsigned int) * appendSize);
				
				unsigned int i = 0;
				while (i < machine->length)
				{
					if (machine->indexes[i] >= program->length || program->states[machine->indexes[i]].indexes[c] == NULL) Machine_halt(machine, i);
					else
					{
						struct State* state = &program->states[machine->indexes[i]];
						machine->indexes[i] = state->indexes[c][0];
						for (unsigned int j = 1; j < state->length[c]; j++)
						{
							if (appendLength >= appendSize)
							{
								appendSize = appendSize * 2;
								append = realloc(append, sizeof(unsigned int) * appendSize);
							}
							append[appendLength] = state->indexes[c][j];
							appendLength += 1;
						}
						i += 1;
					}
				}
				for (unsigned int i = 0; i < appendLength; i++) Machine_start(machine, append[i]);
				free(append);
			}
		}
	}
}

int main ()
{
	struct Machine machine;
	Machine_init(&machine);
	Machine_setup(&machine);
	
	unsigned int inputSkip = 0;
	unsigned int inputSize = 1024;
	char* input = malloc(sizeof(char) * inputSize);
	unsigned int inputLen = 0;
	
	system("color"); // for windows
	
	help();
	
	while (true)
	{
		if (inputSkip == 0)
		{
			putchar('\n');
			Machine_render(&machine);
			putchar('\n');
			char* result = fgets(input, inputSize, stdin);
			if (result == NULL) break;
		}
		else inputSkip -= 1;
		inputLen = strlen(input);
		
		if (strncmp(input, "help", 4) == 0)
		{
			printf("help - Lists and explains commands.\n");
			printf("load {path=\"%s\"} - Loads a program from a file.\n", SAVEPATH);
			printf("save {path=\"%s\"} - Saves the program to a file.\n", SAVEPATH);
			printf("kill - Forcefully terminate all branches.\n");
			if (NEW) printf("bump {index=0} {count=1} - Bumps branch count of the indexed state.\n");
			if (OLD) printf("start {index=0} - Appends specified state index to active branch list.\n");
			printf("empty - Frees program from memory.\n");
			printf("blank - Equivalent to \"resize %d\".\n", BLANKSIZE);
			printf("resize {size} - Resizes the state count to the specified size.\n");
			printf("program {index=%d} {c='%c'} {target=index} - Adds/removes a state transition.\n", machine.program->length - 1, EMPTYCHAR);
			printf("language - Prints all valid characters. Invalid characters get mapped to '%c'.\n", INVALIDCHAR);
			printf("togglebump - Toggles the bump command and branches created by it.\n");
			printf("togglestart - Toggles the start command and branches created by it.\n");
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
				Program_loadCSV(machine.program, file);
				fclose(file);
				
				printf("Loaded from \"%s\".\n", path);
			}
			else printf("Could not load file \"%s\".\n", path);
			
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
			
			FILE* file = fopen(path, "w");
			if (file != NULL)
			{
				Program_saveCSV(machine.program, file);
				fclose(file);
				
				printf("Saved to \"%s\".\n", path);
			}
			else printf("Failed to save to \"%s\".\n", path);
			
			free(path);
		}
		else if (strncmp(input, "kill", 4) == 0)
		{
			Machine_kill(&machine);
		}
		else if (NEW && strncmp(input, "bump", 4) == 0)
		{
			unsigned int index = 0;
			unsigned int count = 1;
			int result = sscanf(input, "bump %d %d ", &index, &count);
			
			if (index < machine.program->length) machine.program->states[index].instances += count;
		}
		else if (OLD && strncmp(input, "start", 5) == 0)
		{
			unsigned int index = 0;
			int result = sscanf(input, "start %d ", &index);
			
			Machine_start(&machine, index);
		}
		else if (strncmp(input, "empty", 5) == 0)
		{
			Program_free(machine.program);
		}
		else if (strncmp(input, "blank", 5) == 0)
		{
			sprintf(input, "resize %d", BLANKSIZE);
			inputSkip = 1;
		}
		else if (strncmp(input, "resize", 6) == 0)
		{
			unsigned int length = 0;
			int result = sscanf(input, "resize %d ", &length);
			
			if (length > MAXPROGRAMSIZE) printf("Program size is capped to %d.\n", MAXPROGRAMSIZE);
			else if (result == 1)
			{
				if (length == machine.program->length) printf("Program is already of size %d.\n", length);
				else if (length < machine.program->length)
				{
					unsigned int delta = machine.program->length - length;
					for (unsigned int i = 0; i < delta; i++) Program_contract(machine.program);
					if (delta == 1) printf("Popped %d state.\n", delta);
					else printf("Popped %d states.\n", delta);
				}
				else
				{
					unsigned int delta = length - machine.program->length;
					for (unsigned int i = 0; i < delta; i++) Program_expand(machine.program);
					if (delta == 1) printf("Appended %d state.\n", delta);
					else printf("Appended %d states.\n", delta);
				}
			}
			else printf("You must specify a size.\n");
		}
		else if (strncmp(input, "program", 7) == 0)
		{
			char c = EMPTYCHAR;
			unsigned int index = machine.program->length - 1;
			unsigned int target = 0;
			int result = sscanf(input, "program %d %c %d ", &index, &c, &target);
			
			if (index < machine.program->length)
			{
				if (result < 3) target = index;
				
				struct State* state = &machine.program->states[index];
				
				bool removed = State_remove(state, c, target);
				if (!removed) State_append(state, c, target);
			}
			else
			{
				printf("State index (%d) out of bounds.\n", index);
			}
		}
		else if (strncmp(input, "language", 8) == 0)
		{
			for (char c = 0; c < LANGSIZE; c++) putchar(unmap(c));
			putchar('\n');
		}
		else if (strncmp(input, "togglebump", 10) == 0)
		{
			NEW = !NEW;
			if (NEW) printf("Computing branches created by the \"bump\" command.\n");
			else printf("Ignoring branches created by bump command.\n");
		}
		else if (strncmp(input, "togglestart", 11) == 0)
		{
			OLD = !OLD;
			if (OLD) printf("Computing branches created by the \"start\" command.\n");
			else printf("Ignoring branches created by the \"start\" command.\n");
		}
		else if (machine.program != NULL && machine.program->length > 0)
		{
			char* s = input;
			char c = *s;
			
			unsigned int instances = machine.length * OLD;
			if (NEW) for (unsigned int i = 0; i < machine.program->length; i++) instances += machine.program->states[i].instances;
			
			if (instances == 0)
			{
				printf("There are no active branches.\n");
				if (NEW) printf("Do %sbump%s to create a branch.\n", X1B_UNDERLINE, X1B_RESET);
				if (OLD) printf("Do %sstart%s to create a branch.\n", X1B_UNDERLINE, X1B_RESET);
				help();
			}
			else
			{
				while ((c = *(s++)) != '\0')
				{
					Machine_tick(&machine, c);
				}
			}
		}
	}
	
	Machine_free(&machine);
	
	return 0;
}