#include <stdio.h>  
#include <stdlib.h>  
#include <string.h>  
#include <unistd.h>  
#include <stdint.h>

enum { EMONCMS, DOMAH } OUTPUT_MODE;

static int PAPP_accum;
static int PAPP_number;
static int BASE_value = -1;

const char *emoncms_fmt = "appart.PAPP:%d\nappart.BASEKWH:%d\n";
const char *hm_fmt = "electricity/%d,%d\n";

const char *output_fmt = NULL;

void accumulate_value(const char *label, const char *value)
{
	int val = atoi(value);
	if (!strcmp(label, "PAPP")) {
		PAPP_accum += val;
		PAPP_number ++;
	} else if (!strcmp(label, "BASE")) {
		BASE_value = val;
	}

	if (PAPP_number == 60) {
		printf(output_fmt, PAPP_accum/PAPP_number, BASE_value/1000);
		PAPP_accum = 0;
		PAPP_number = 0;
	}
}
/*          sum = 32
                for c in etiquette: sum = sum + ord(c)
                for c in valeur:        sum = sum + ord(c)
                sum = (sum & 63) + 32
                return chr(sum)
*/

char compute_cksum(const char *label, const char *value)
{
	int sum = 32;
	int i;
	for (i = 0; i < strlen(label); i++) {
		sum += label[i];
	}
	for (i = 0; i < strlen(value); i++) {
		sum += value[i];
	}

	sum = (sum & 63) + 32;

	return sum;
}

static void parse_message(char *msg)
{
	char *label = msg;
	char *value = strchr(label, ' ');
	char *cksum;
	if (!value) {
		fprintf(stderr, "Did not find value in message \"%s\"\n", msg);
		return;
	}
	*value = 0;
	value++;

	cksum = strchr(value, ' ');
	if (!cksum) {
		fprintf(stderr, "Did not find cksum in message \"%s\"\n", msg);
		return;
	}

	*cksum = 0;
	cksum++;

	if (*cksum != compute_cksum(label, value)) {
		fprintf(stderr, "%s %s has cksum %c, invalid on the wire.\n", label, value, *cksum);
		return;
	} else {
//		fprintf(stderr, "%s %s has valid cksum %c\n", label, value, *cksum);
		accumulate_value(label, value);
	}

}

int main(int argc, char **argv)
{

	setvbuf(stdin, NULL, _IONBF, 0);
	setvbuf(stdout, NULL, _IONBF, 0);
	
	if (argc == 2 && !strcmp(argv[1], "--emoncms"))
		output_fmt = emoncms_fmt;
	else output_fmt = hm_fmt;
	
	char buf[4096];
	while (!feof(stdin)) {
		if (!fgets(&buf[0], 4095, stdin))
			sleep(1);
		parse_message(&buf[0]);
	}


}

