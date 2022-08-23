#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>

int main(int argc, char* argv[])
{
	char *end;
	unsigned long long value = strtoull(argv[argc - 1], &end, 10);
	unsigned long long new_value = round(sqrt(value));
	char result[25];
	int len = sprintf(result, "%llu", new_value);
	result[len] = '\0';
	if(argc == 2)
		printf("%s\n", result);
	else {
		argv[argc - 1] = result;
		if(strcmp("double", argv[1]) == 0)
			execv("./double", argv + 1);
		else if(strcmp("square", argv[1]) == 0)
			execv("./square", argv + 1);
		else if(strcmp("root", argv[1]) == 0)
			execv("./root", argv + 1);
		else {
			printf("UNABLE TO EXECUTE\n");
			exit(-1);
		}
	}
	return 0;
}
