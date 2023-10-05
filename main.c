/**
 * Tony Givargis
 * Copyright (C), 2023
 * University of California, Irvine
 *
 * CS 238P - Operating Systems
 * main.c
 */

#include "jitc.h"
#include "parser.h"
#include "system.h"

/* export LD_LIBRARY_PATH=. */

int varId = 0;

int genFuncBodyFromDag(const struct parser_dag *dag, FILE *file) {
        if (dag) {
                int leftVarId = genFuncBodyFromDag(dag->left, file);
                int rightVarId = genFuncBodyFromDag(dag->right, file);

                /* based on type of the node, generate the code */
                switch (dag->op) {
                        case PARSER_DAG_:
                                return -1;
                        case PARSER_DAG_VAL:
                                fprintf(file, "double t%d = %f;\n", varId, dag->val);
                                break;
                        case PARSER_DAG_NEG:
                                fprintf(file, "double t%d = -1 * t%d;\n", varId, rightVarId);
                                break;
                        case PARSER_DAG_MUL:
                                fprintf(file, "double t%d = t%d * t%d;\n", varId, leftVarId, rightVarId);
                                break;
                        case PARSER_DAG_DIV:
                                fprintf(file, "double t%d = t%d ? (t%d / t%d) : 0.0;\n", varId, rightVarId, leftVarId, rightVarId);
                                break;
                        case PARSER_DAG_ADD:
                                fprintf(file, "double t%d = t%d + t%d;\n", varId, leftVarId, rightVarId);
                                break;
                        case PARSER_DAG_SUB:
                                fprintf(file, "double t%d = t%d - t%d;\n", varId, leftVarId, rightVarId);
                                break;
                }
                varId++;
                return (varId - 1);
        }
        return -1;
}

static void
generate(const struct parser_dag *dag, FILE *file)
{
	/* YOUR CODE HERE */
        int valueVarId;
        fprintf(file, "double evaluate(void) {\n");
        valueVarId = genFuncBodyFromDag(dag, file);
        fprintf(file, "return t%d;\n", valueVarId);
        fprintf(file, "}\n");
}

typedef double (*evaluate_t)(void);

int
main(int argc, char *argv[])
{
	const char *CFILE = "out.c";
        const char *SOFILE = "out.so";
	struct parser *parser;
	struct jitc *jitc;
	evaluate_t fnc;
	FILE *file;

	/* usage */

	if (2 != argc) {
		printf("usage: %s expression\n", argv[0]);
		return -1;
	}

	/* parse */

	if (!(parser = parser_open(argv[1]))) {
		TRACE(0);
		return -1;
	}

	/* generate C */

	if (!(file = fopen(CFILE, "w"))) {
		TRACE("fopen()");
		return -1;
	}
	generate(parser_dag(parser), file);
	parser_close(parser);
	fclose(file);

	/* JIT compile */

	if (jitc_compile(CFILE, SOFILE)) {
		file_delete(CFILE);
		TRACE(0);
		return -1;
	}
	file_delete(CFILE);

	/* dynamic load */
	if (!(jitc = jitc_open(SOFILE)) ||
	    !(fnc = (evaluate_t)jitc_lookup(jitc, "evaluate"))) {
		file_delete(SOFILE);
		jitc_close(jitc);
		TRACE(0);
		return -1;
	}
	printf("%f\n", fnc());
	
        /* done */
	
        file_delete(SOFILE);
	jitc_close(jitc);
	
        return 0;
}

