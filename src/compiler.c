#include "../includes/compiler.h"
#include "../includes/chunk.h"
#include "../includes/scanner.h"
#include "../includes/vm.h"
#include "../includes/debug.h"
#include "../includes/value.h"
#include "../includes/object.h"
#include "../includes/memory.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static void assignmentStatement(Scanner* scanner, Parser* parser, int type);
static void callStatement(Scanner* scanner, Parser* parser);
static void statement(Scanner* scanner, Parser* parser);
static void expression(Scanner* scanner, Parser* parser);
static void and(Scanner* scanner, Parser* parser);
static void or(Scanner* scanner, Parser* parser);
static void equality(Scanner* scanner, Parser* parser);
static void comparison(Scanner* scanner, Parser* parser);
static void term(Scanner* scanner, Parser* parser);
static void factor(Scanner* scanner, Parser* parser);
static void highestBinary(Scanner* scanner, Parser* parser);
static void unary(Scanner* scanner, Parser* parser);
static void call(Scanner* scanner, Parser* parser);
static void primary(Scanner* scanner, Parser* parser);

/* Helper functions */ 
static bool checkPrimary(Scanner* scanner, Parser* parser);
static void parseNumber(Scanner* scanner, Parser* parser);
static void parseString(Scanner* scanner, Parser* parser);
static void parseBoolean(Scanner* scanner, Parser* parser, bool value);
static void parseNil(Scanner* scanner, Parser* parser);
static void parseArray(Scanner* scanner, Parser* parser);
static void parseTable(Scanner* scanner, Parser* parser);
static bool parseReadIdentifier(Scanner* scanner, Parser* parser, Token identifier);
static void parseGrouping(Scanner* scanner, Parser* parser);
static void parseGlobalDeclaration(Scanner* scanner, Parser* parser); 
static void parseVariableDeclaration(Scanner* scanner, Parser* parser);
static void parseIdentifier(Parser* parser, Token identifier, uint8_t normins, uint8_t longins);
static void parseBlock(Scanner* scanner, Parser* parser);
static void parseLocalDeclaration(Scanner* scanner, Parser* parser);
static bool identifiersEqual(Token id1, Token id2);
static int resolveLocal(Compiler* compiler, Token identifier);
static int resolveUpvalue(Parser* parser, Compiler* compiler, Token identifier);
static void parseWhileStatement(Scanner* scanner, Parser* parser);
static void parseIfStatement(Scanner* scanner, Parser* parser);
static void parseBreakStatement(Scanner* scanner, Parser* parser);
static void parseForStatement(Scanner* scanner, Parser* parser);
static void endCompiler(Compiler* compiler, Parser* parser, uint8_t ins);
static void patchMisc(Parser* parser, unsigned int index, uint8_t value); 
static int parseParameters(Scanner* scanner, Parser* parser, bool* variadicFlag); 
static void beginScope(Parser* parser);
static int endScope(Parser* parser);
static void parseClosureFunction(Scanner* scanner, Parser* parser, Token identifier, FunctionType type);
static void parseSupercall(Scanner* scanner, Parser* parser, bool shouldReturn);

Token token_super = {TOKEN_IDENTIFIER, "super", 5, 0};
Token token_self = {TOKEN_IDENTIFIER, "self", 4, 0};
Token token_empty = {TOKEN_IDENTIFIER, "", 0, 0};

void initCompiler(Compiler* compiler, ObjFunction* function, FunctionType type ) { 
    compiler->localCount = 1;
    compiler->scopeDepth = 0;
    compiler->significantTemps = 0;
    compiler->function = function;
    compiler->functionType = type;
    compiler->enclosing = NULL;

    Local local;
    local.depth = 0;
    local.identifier = type == TYPE_NORMAL ? token_empty : token_self;
    local.isCaptured = false; 
    local.significantTemps = 0;

    compiler->locals[0] = local;
}

void setCompiler(Compiler* compiler, Parser* parser) {
    compiler->enclosing = parser->compiler;
    parser->compiler = compiler;
}

void initParser(Parser* parser, Compiler* compiler, VM* vm) {
    parser->hadError = false;
    parser->panicMode = false;
    parser->vm = vm;
    parser->compiler = compiler;
    parser->unpatchedBreaks = NULL;
}

void initUintArray(UintArray* array) {
    array->array = NULL;
    array->capacity = 0;
    array->count = 0;
}

void writeUintArray(UintArray* array, unsigned int value) {
    if (array->capacity < array->count + 1) {
        int oldCapacity = array->capacity;
        array->capacity = GROW_CAPACITY(oldCapacity);
        array->array = GROW_ARRAY(unsigned int, array->array, oldCapacity, array->capacity);
    }


    array->array[array->count] = value;
    array->count++;
}

unsigned int getUintArray(UintArray* array, int index) {
    if (index + 1 > array->count) {
        printf("Internal Error : Index out of range\n");
        return 0;
    }

    return array->array[index];
}

void freeUintArray(UintArray* array) {
    FREE_ARRAY(unsigned int, array->array, array->capacity);
    initUintArray(array);
}

static void errorAt(Parser* parser, Token* token, const char* message) {
    if (parser->panicMode == true) {
        return;
    }
    /* Suppress */
    parser->panicMode = true;
    fprintf(stderr, "Error on line %d : ", token->type == TOKEN_EOF ? token->line - 1 : token->line);

    if (token->type == TOKEN_EOF) {
        fprintf(stderr, "At End");
    } else if (token->type != TOKEN_ERROR) {
        /* Everything else */
        fprintf(stderr, "At '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
    parser->hadError = true;
}


static void errorAtCurrent(Parser* parser, const char* message) {
    errorAt(parser, &parser->current, message);
}

static void error(Parser* parser, const char* message) {
    errorAt(parser, &parser->previous, message);
}

static void advance(Scanner* scanner, Parser* parser) {
    parser->previous = parser->current;

    for (;;) {
        parser->current = scanToken(scanner);
        if (parser->current.type != TOKEN_ERROR) break;

        errorAtCurrent(parser, parser->current.start);
    }
}

static bool match(Scanner* scanner, Parser* parser, TokenType type) {
    if (parser->current.type != type) return false;
    advance(scanner, parser);
    return true;
}

static void consume(Scanner* scanner, Parser* parser, TokenType type, const char* message) {
    if (parser->current.type == type) {
        advance(scanner, parser);
        return;
    }

    errorAtCurrent(parser, message);
}

static Chunk* currentChunk(Parser* parser) {
    return &parser->compiler->function->chunk;
}

static void emitByte(Parser* parser, uint8_t byte) {
    writeChunk(currentChunk(parser), byte, parser->previous.line); 
}

static void emitBytes(Parser* parser, uint8_t byte1, uint8_t byte2) {
    /* Meant for easy operand passing */
    emitByte(parser, byte1);
    emitByte(parser, byte2);
}

static void emitLongOperand(Parser* parser, uint16_t op, uint8_t ins1, uint8_t ins2) {
    /* Shrinks the operand down if possible */
    if (op <= UINT8_MAX) {
        emitBytes(parser, ins1, (uint8_t)op);
    } else {
        emitByte(parser, ins2);
        writeLongByte(currentChunk(parser), op, parser->previous.line);
    }
}

static unsigned int emitJump(Parser* parser, uint8_t instruction) {
    unsigned int index = currentChunk(parser)->elem_count - 1;
    emitByte(parser, instruction);
    emitBytes(parser, 0xff, 0xff);
    return index + 2;       // return index of first operand
}

static void patchMisc(Parser* parser, unsigned int index, uint8_t value) {
    currentChunk(parser)->code[index] = value;
}

static void patch(Parser* parser, unsigned int index) {
    /* Takes index of the first byte of jump instruction, and patches it to the current location */
    int extra = -1;

    int currentIndex = currentChunk(parser)->elem_count - 1;
    writeLongByteAt(currentChunk(parser), currentIndex - index + extra, index, parser->previous.line); 
}

static void patchBreak(Parser* parser, unsigned int index, uint8_t localCount) {
    /* Takes index of jump instruction's first operand byte and calculates the location of the 
     * popn operand, then fills it with the appropriate count to pop 
     * the locals */ 

    unsigned int operand = index - 2; 
    currentChunk(parser)->code[operand] = localCount;
}

static void emitJumpBack(Parser* parser, unsigned int index) {

    /* Compiles a jump instruction that jumps back to the given index */
    int currentIndex = currentChunk(parser)->elem_count - 1;
    uint16_t offset = currentIndex - index;
    emitByte(parser, OP_JMP_BACK);
    // + 3 is the extra offset required by OP_JMP_BACK 
    writeLongByte(currentChunk(parser), offset + 3, parser->previous.line);
}

static void emitClosureEncoding(Parser* parser, Compiler* compiler) {
    for (int i = 0; i < compiler->function->upvalueCount; i++) {
        emitByte(parser, compiler->upvalues[i].isLocal ? 1 : 0);
        emitByte(parser, (uint8_t)compiler->upvalues[i].index);
    }
}

static void checkBadExpression(Scanner* scanner, Parser* parser) {
    /* If next token is a primary after a full operator
     * sequence, report an error */
    if (checkPrimary(scanner, parser)) {
        errorAtCurrent(parser, "Unexpected literal in operator sequence (Syntax Error)");
        return;
    }
}

static void expression(Scanner* scanner, Parser* parser) {
    /* if (!checkPrimary(scanner, parser)) return;          If we dont find a primary expression, 
                                                           we arent going to be parsing any significant ex */
    or(scanner, parser);
}

static void or(Scanner* scanner, Parser* parser) {
    and(scanner, parser);

    while (parser->current.type == TOKEN_OR) {
        advance(scanner, parser);
        int jumpIndex = emitJump(parser, OP_JMP_OR); 
        and(scanner, parser);
        patch(parser, jumpIndex);
    }
}

static void and(Scanner* scanner, Parser* parser) {
    equality(scanner, parser); 

    while (parser->current.type == TOKEN_AND) {
        advance(scanner, parser);
        int jumpIndex = emitJump(parser, OP_JMP_AND);
        equality(scanner, parser);
        patch(parser, jumpIndex);
    }
}

static void equality(Scanner* scanner, Parser* parser) {
     /* Parse the left operand */
    comparison(scanner, parser);  

    while (match(scanner, parser, TOKEN_EQUAL_EQUAL) ||
           match(scanner, parser, TOKEN_BANG_EQUAL)) {
        
        TokenType type = parser->previous.type;
        /* Parse the right operand */
        comparison(scanner, parser);

        switch (type) {
            case TOKEN_EQUAL_EQUAL:
                emitByte(parser, OP_EQUAL);
                break;
            case TOKEN_BANG_EQUAL:
                emitByte(parser, OP_NOT_EQ);
                break;
            default: break;
        }
    }

    // checkBadExpression(scanner, parser);
}

static void comparison(Scanner* scanner, Parser* parser) {
    /* Parse the left operand */ 
    term(scanner, parser);

    while (match(scanner, parser, TOKEN_GREATER) ||
           match(scanner, parser, TOKEN_GREATER_EQUAL) ||
           match(scanner, parser, TOKEN_LESS) || 
           match(scanner, parser, TOKEN_LESS_EQUAL)) {

        TokenType type = parser->previous.type;
        /* Parse the right operand */
        term(scanner, parser);

        switch (type) {
            case TOKEN_GREATER:
                emitByte(parser, OP_GREATER); 
                break;
            case TOKEN_LESS:
                emitByte(parser, OP_LESSER);
                break;
            case TOKEN_LESS_EQUAL:
                emitByte(parser, OP_LESSER_EQ);
                break;
            case TOKEN_GREATER_EQUAL:
                emitByte(parser, OP_GREATER_EQ);
                break;
            default: break;
        }
    }

    // checkBadExpression(scanner, parser);
}

static void term(Scanner* scanner, Parser* parser) {
    /* Parse the left operand */
    factor(scanner, parser);

    while (match(scanner, parser, TOKEN_PLUS) ||
           match(scanner, parser, TOKEN_MINUS)) {

        TokenType type = parser->previous.type;

        /* Parse the right operand */ 
        factor(scanner, parser);

        switch (type) {
            case TOKEN_PLUS:
                emitByte(parser, OP_ADD);
                break;
            case TOKEN_MINUS:
                emitByte(parser, OP_SUB);
                break;
            default: break;
        }
    }

    // checkBadExpression(scanner, parser);
}

static void factor(Scanner* scanner, Parser* parser) {
    /* Parse the left operand */
    highestBinary(scanner, parser);

    while (match(scanner, parser, TOKEN_ASTERISK) ||
           match(scanner, parser, TOKEN_SLASH)) {

        TokenType type = parser->previous.type;
        highestBinary(scanner, parser);

        switch (type) {
            case TOKEN_ASTERISK:
                emitByte(parser, OP_MUL);
                break;
            case TOKEN_SLASH:
                emitByte(parser, OP_DIV);
                break;
            default: break;
        }
    }

    // checkBadExpression(scanner, parser);
}

static void highestBinary(Scanner* scanner, Parser* parser) {
    unary(scanner, parser);
    while (match(scanner, parser, TOKEN_EXP)) {
        TokenType type = parser->previous.type;
        unary(scanner, parser);
        
        switch (type) {
            case TOKEN_EXP:
                emitByte(parser, OP_POW);
                break;
            default: return;
        }
    }

    // checkBadExpression(scanner, parser);
}

static void unary(Scanner* scanner, Parser* parser) {
    if (match(scanner, parser, TOKEN_BANG) ||
        match(scanner, parser, TOKEN_MINUS)||
        match(scanner, parser, TOKEN_HASH)) {
        TokenType type = parser->previous.type;
        call(scanner, parser);
        switch (type) {
            case TOKEN_BANG:
                emitByte(parser, OP_NOT);
                break;
            case TOKEN_MINUS:
                emitByte(parser, OP_NEGATE);
                break;
            case TOKEN_HASH:
                emitByte(parser, OP_LENGTH);
                break;
            default:
                return;
        }
        // checkBadExpression(scanner, parser);
    } else {
        /* Head to primary */
        call(scanner, parser);
    }
}

static void parseArrayIndex(Scanner* scanner, Parser* parser) {
    advance(scanner, parser);       /* '[' */ 
    expression(scanner, parser);
    consume(scanner, parser, TOKEN_SQUARE_CLOSE, "Expected a ']' after array index");
}

static int parseFunctionArguments(Scanner* scanner, Parser* parser) {
    advance(scanner, parser);
    int arity = 0;
    if (parser->current.type != TOKEN_ROUND_CLOSE) {
        do {
             expression(scanner, parser);
             arity++;
        } while (match(scanner, parser, TOKEN_COMMA));
    }
    consume(scanner, parser, TOKEN_ROUND_CLOSE, "Expected an ')' while parsing arguments");
    if (arity > LVAR_MAX - 1) {
        error(parser, "Cannot have more than 255 arguments");
    }
    return arity;
}

static void parseDirectCallSequence(Scanner* scanner, Parser* parser) { 
    // Used when evaluating expressions as opposed to assigmen t
    if (parser->previous.type == TOKEN_END) {
        errorAtCurrent(parser, "Unexpected Call");
        return;
    }
    switch (parser->current.type) {
        case TOKEN_SQUARE_OPEN:
            // Parses [exp]
            parseArrayIndex(scanner, parser);
            emitByte(parser, OP_CUSTOM_INDEX_GET);
            parseDirectCallSequence(scanner, parser);
            break;
        case TOKEN_ROUND_OPEN: {
            uint8_t arity = (uint8_t)parseFunctionArguments(scanner, parser);
            emitBytes(parser, OP_CALL, arity);
            emitByte(parser, 1);                // it does return 
            parseDirectCallSequence(scanner, parser);
            break;
        }
        case TOKEN_DOT: {
            advance(scanner, parser);
            consume(scanner, parser, TOKEN_IDENTIFIER, "Expected Identifier in call sequence");
            Token identifier = parser->previous;
            
            int index = makeConstant(currentChunk(parser), 
                    OBJ(allocateString(parser->vm, identifier.start, identifier.length)));
            
            
            if (parser->current.type == TOKEN_ROUND_OPEN) {
                // direct method call optimisation 
                
                int arity = parseFunctionArguments(scanner, parser);
                emitLongOperand(parser, (uint16_t)index, OP_CONST, OP_CONST_LONG);

                emitByte(parser, OP_INVOKE);
                emitBytes(parser, (uint8_t)arity, 1);
                parseDirectCallSequence(scanner, parser);
                break;
            }

            emitLongOperand(parser, (uint16_t)index, OP_CONST, OP_CONST_LONG);
            emitByte(parser, OP_GET_FIELD);
            parseDirectCallSequence(scanner, parser);
            break;
        }
        default: break;
    }
}

static bool checkCall(Scanner* scanner, Parser* parser) {
    switch (parser->current.type) {
        case TOKEN_SQUARE_OPEN:
        case TOKEN_DOT:
        case TOKEN_ROUND_OPEN:
            return true;
        default: return false;
    }

    /* Need to add more later */

}

static int parseCallSequenceField(Scanner* scanner, Parser* parser, bool doesReturn) {
    /* This function parses all the calls normally, it parses the last one too but 
     * instead of emitting an instruction, it returns the type of the call */ 
    
    switch (parser->current.type) {
        case TOKEN_SQUARE_OPEN: {
            parseArrayIndex(scanner, parser);
            if (checkCall(scanner, parser)) {
                emitByte(parser, OP_CUSTOM_INDEX_GET);
                return parseCallSequenceField(scanner, parser, doesReturn);
            } else {
                /* We've reached the end, this is the target */
                return CALL_ARRAY;
            }

        }
        case TOKEN_ROUND_OPEN: {
            /* In case of an invokcation call, we always parse it 
             * no matter what, we return CALL_FUNC to notify this cannot be an 
             * assignment */

            uint8_t arity = (uint8_t)parseFunctionArguments(scanner, parser);
            emitByte(parser, OP_CALL);
            emitByte(parser, arity);
            
            if (checkCall(scanner, parser)) {
                emitByte(parser, 1);
                return parseCallSequenceField(scanner, parser, doesReturn);
            } else {
                emitByte(parser, (uint8_t)doesReturn);
                return CALL_FUNC;
            }
        }
        case TOKEN_DOT: {
            advance(scanner, parser);
            consume(scanner, parser, TOKEN_IDENTIFIER, "Expected an identifier while assigning field");
            Token identifier = parser->previous;
            int index = makeConstant(currentChunk(parser), 
                    OBJ(allocateString(parser->vm, identifier.start, identifier.length)));
           
            if (checkCall(scanner, parser)) {
                /* Consider this a get expression of a field index */ 

                if (parser->current.type == TOKEN_ROUND_OPEN) {
                    // direct method call optimisation 
                    int arity = parseFunctionArguments(scanner, parser);
                    emitLongOperand(parser, (uint16_t)index, OP_CONST, OP_CONST_LONG);


                    emitByte(parser, OP_INVOKE);
                    emitByte(parser, (uint8_t)arity);

                    if (checkCall(scanner, parser)) {
                        emitByte(parser, 1);
                        return parseCallSequenceField(scanner, parser, doesReturn);
                    } else {
                        emitByte(parser, doesReturn);
                        return CALL_FUNC;
                    }
                } 
                emitLongOperand(parser, (uint16_t)index, OP_CONST, OP_CONST_LONG);
                emitByte(parser, OP_GET_FIELD);    
                return parseCallSequenceField(scanner, parser, doesReturn);
            } else { 
                /* We have parsed as much as possible, this is now an assignment target */
                emitLongOperand(parser, (uint16_t)index, OP_CONST, OP_CONST_LONG);
                return CALL_DOT;
            }
        }
        /* We cant find a valueable token after consuming the identifier
         * so we return CALL_NONE to assume it was standalone*/
        default: return CALL_NONE;
    }
}

static void call(Scanner* scanner, Parser* parser) {
    primary(scanner, parser);

    if (checkCall(scanner, parser)) {
        parseDirectCallSequence(scanner, parser);
    }
   
}

static void primary(Scanner* scanner, Parser* parser) {
    switch (parser->current.type) {
        case TOKEN_ROUND_OPEN:
            parseGrouping(scanner, parser);
            break;
        case TOKEN_NUMBER:
            parseNumber(scanner, parser);
            break;
        case TOKEN_STRING:
            parseString(scanner, parser);
            break;
        case TOKEN_FALSE:
            parseBoolean(scanner, parser, false);
            break;
        case TOKEN_TRUE:
            parseBoolean(scanner, parser, true);
            break;
        case TOKEN_NIL:
            parseNil(scanner, parser);
            break;
        case TOKEN_IDENTIFIER:
            advance(scanner, parser);
            parseReadIdentifier(scanner, parser, parser->previous); 
            break;
        case TOKEN_SELF:
            advance(scanner, parser);
            parseReadIdentifier(scanner, parser, parser->previous);
            break;
        case TOKEN_SUPER:
            advance(scanner, parser);
            parseSupercall(scanner, parser, true);
            break;
        case TOKEN_SQUARE_OPEN:
            parseArray(scanner, parser);
            break;
        case TOKEN_CURLY_OPEN:
            parseTable(scanner, parser);
            break;
        case TOKEN_FUNC: 
            advance(scanner, parser);
            
            Token name;
            name.start = "function";
            name.length = 8;
            name.type = TOKEN_IDENTIFIER;
            name.line = parser->previous.line;

            parseClosureFunction(scanner, parser, name, TYPE_NORMAL); 
            break;
        default:
            error(parser, "Expected Expression, got (Syntax Error)");
            return;
    }
}

static bool checkPrimary(Scanner* scanner, Parser* parser) {
    switch (parser->current.type) {
        case TOKEN_NUMBER:
        case TOKEN_STRING:
        case TOKEN_FALSE:
        case TOKEN_TRUE:
        case TOKEN_NIL:
        case TOKEN_IDENTIFIER:
        case TOKEN_SELF:
        case TOKEN_SUPER:
        case TOKEN_ROUND_OPEN:
        /* unaries */
        case TOKEN_MINUS:
        case TOKEN_BANG:
        case TOKEN_FUNC:
        case TOKEN_SQUARE_OPEN:
        case TOKEN_CURLY_OPEN:
            return true;
        default: return false;
    }
}

static void parseNumber(Scanner* scanner, Parser* parser) {
    advance(scanner, parser);
    double num = strtod(parser->previous.start, NULL);
    writeConstant(currentChunk(parser), NATIVE_TO_NUMBER(num), parser->previous.line); 
}

static void parseString(Scanner* scanner, Parser* parser) {
    advance(scanner, parser);

    /* Parsing escape sequences */ 
    char arrayStr[parser->previous.length - 2];     // no need to store quotes 
    
    int length = 0;
    for (int i = 1; i < parser->previous.length - 1; i++) {
        switch (parser->previous.start[i]) {
            case '\\':
                if (i + 1 > parser->previous.length - 1) {
                    // incase the backslash was the last character 
                    error(parser, "Incomplete escape sequence in string");
                    return;
                }

                switch (parser->previous.start[i + 1]) {
                    case 'n':
                        arrayStr[length] = '\n';
                        i++;
                        length++;
                        break;
                    case '\\':
                        arrayStr[length] = '\\';
                        i++;
                        length++;
                        break;
                    case 't':
                        arrayStr[length] = '\t';
                        i++;
                        length++;
                        break;
                    case 'b':
                        arrayStr[length] = '\b';
                        i++;
                        length++;
                        break;
                    case '\"':
                        arrayStr[length] = '\"';
                        i++;
                        length++;
                        break;
                    case '\'':
                        arrayStr[length] = '\'';
                        i++;
                        length++;
                        break;
                    default: 
                        error(parser, "Incomplete escape sequence in string");
                        return;

                }
                break;
            default:
                arrayStr[length] = parser->previous.start[i];
                length++;
        }
    }

    Value value = OBJ(allocateString(parser->vm, arrayStr, length));
    writeConstant(currentChunk(parser), value, parser->previous.line);
}

static void parseBoolean(Scanner* scanner, Parser* parser, bool value) {
    advance(scanner, parser);
    value ? emitByte(parser, OP_TRUE) : emitByte(parser, OP_FALSE);
}

static void parseNil(Scanner* scanner, Parser* parser) {
    advance(scanner, parser);
    emitByte(parser, OP_NIL);
}

static void parseArray(Scanner* scanner, Parser* parser) {
    advance(scanner, parser);           /* Consume the '[' */ 
    
    bool canParseRange = true;

    emitByte(parser, OP_ARRAY);         /* Push the array */
    if (match(scanner, parser, TOKEN_SQUARE_CLOSE)) {
        return;
    }

    /* Push initial elements */
    do {
        expression(scanner, parser);

        if (parser->current.type == TOKEN_RANGE) {
            if (!canParseRange) {
                errorAtCurrent(parser, "Unexpected Range Token");
                break;
            }

            advance(scanner, parser);
            expression(scanner, parser);

            if (match(scanner, parser, TOKEN_RANGE)) {
                expression(scanner, parser);
            } else {
                emitByte(parser, OP_PLUS1);
            }
            emitByte(parser, OP_ARRAY_RANGE);
            canParseRange = false;
        } else {
            emitByte(parser, OP_ARRAY_INS);
            canParseRange = true;
        }
    } while (match(scanner, parser, TOKEN_COMMA) && parser->current.type != TOKEN_EOF);

    if (parser->current.type == TOKEN_EOF) {
        errorAtCurrent(parser, "Unclosed Array Definition");
    }

    consume(scanner, parser, TOKEN_SQUARE_CLOSE, "Expected to close array definition with ']'");
}

static void parseTable(Scanner* scanner, Parser* parser) {
    advance(scanner, parser);
    emitByte(parser, OP_TABLE);
    
    if (match(scanner, parser, TOKEN_CURLY_CLOSE)) return;

    do {
        consume(scanner, parser, TOKEN_STRING, "Expected a string for table key");
        if (parser->previous.type != TOKEN_STRING) return;

        uint16_t constant = makeConstant(currentChunk(parser), 
                OBJ(allocateString(parser->vm, parser->previous.start + 1, parser->previous.length - 2)));
        
        consume(scanner, parser, TOKEN_EQUAL, "Expected an '=' when assigning keys");
        expression(scanner, parser);
        emitLongOperand(parser, constant, OP_TABLE_INS, OP_TABLE_INS_LONG);
    } while (match(scanner, parser, TOKEN_COMMA));

    consume(scanner, parser, TOKEN_CURLY_CLOSE, "Expected to close table definition with '}'");
}

static void parseIdentifier(Parser* parser, Token identifier, uint8_t normins, uint8_t longins) {
    Value value = OBJ(allocateString(parser->vm, identifier.start, identifier.length));
    uint16_t index = makeConstant(currentChunk(parser), value);

    emitLongOperand(parser, index, normins, longins);
}

static bool parseReadIdentifier(Scanner* scanner, Parser* parser, Token identifier) {
    int localIndex = resolveLocal(parser->compiler, identifier);

    if (localIndex != -1) {
        emitBytes(parser, OP_GET_LOCAL, (uint8_t)localIndex);
        return true; 
    } 

    int upvalueIndex = resolveUpvalue(parser, parser->compiler, identifier);
    if (upvalueIndex != -1) {
        emitBytes(parser, OP_GET_UPVALUE, (uint8_t)upvalueIndex);
        return true;
    }
    
    parseIdentifier(parser, identifier, OP_GET_GLOBAL, OP_GET_LONG_GLOBAL); 

    return false;       // possibly not found 
}

static void parseGrouping(Scanner* scanner, Parser* parser) {
    advance(scanner, parser);       /* Consume '(' */ 
    expression(scanner, parser);
    consume(scanner, parser, TOKEN_ROUND_CLOSE, "Expected a ')' to close expression (Syntax Error)"); 
}

static void parseGlobalDeclaration(Scanner* scanner, Parser* parser) {
    advance(scanner, parser);           // consume 'global'
    consume(scanner, parser, TOKEN_IDENTIFIER, "Expected Identifier in global declaration");
    Token identifier = parser->previous;

    if (match(scanner, parser, TOKEN_EQUAL)) {
        expression(scanner, parser); 
    } else {
        emitByte(parser, OP_NIL);
    }

    parseIdentifier(parser, identifier, OP_DEFINE_GLOBAL, OP_DEFINE_LONG_GLOBAL);
    match(scanner, parser, TOKEN_SEMICOLON);
}

static void beginScope(Parser* parser) {
    parser->compiler->scopeDepth++;
}

static int endScope(Parser* parser) {
    int consecutivePops = 0;
    int count = 0;

    for (int i = parser->compiler->localCount - 1; i >= 0; i--) {
        Local local = parser->compiler->locals[i];
        if (local.depth != parser->compiler->scopeDepth) break;
        
        if (local.isCaptured) {
            if (consecutivePops > 0) {
                emitBytes(parser, OP_POPN, (uint8_t)consecutivePops);
                consecutivePops = 0;
            }
            emitByte(parser, OP_CLOSE_UPVALUE);
        } else {
            consecutivePops++;
        }
        count++;
        parser->compiler->localCount--;
    }

    if (consecutivePops > 0) {
        emitBytes(parser, OP_POPN, (uint8_t)consecutivePops);
    }

    parser->compiler->scopeDepth--;

    return count;

}

static int addUpvalue(Parser* parser, Compiler* compiler, int index, bool isLocal) {
    if (compiler->function->upvalueCount == UPVAL_MAX) {
        error(parser, "Cannot close over more than 256 variables");
        return -1;
    }

    // Re-Use upvalues which are duplicates

    for (int i = 0; i < compiler->function->upvalueCount; i++) {
        Upvalue value = compiler->upvalues[i];
        if (value.index == index && value.isLocal == isLocal) {
            return i;
        }
    }

    Upvalue upvalue;
    upvalue.index = (uint8_t)index;
    upvalue.isLocal = isLocal;
    compiler->upvalues[compiler->function->upvalueCount] = upvalue;
    return compiler->function->upvalueCount++;
}

static int resolveUpvalue(Parser* parser, Compiler* compiler, Token identifier) {
    if (compiler->enclosing == NULL) return -1;

    int localIndex = resolveLocal(compiler->enclosing, identifier);
    if (localIndex != -1) {
        // returns index of the upvalue which was just added
        compiler->enclosing->locals[localIndex].isCaptured = true;
        // mark variable as captured 
        return addUpvalue(parser, compiler, localIndex, true);
    }

    int upvalueIndex = resolveUpvalue(parser, compiler->enclosing, identifier);
    if (upvalueIndex != -1) {
        // recursive upvalue sequence 
        return addUpvalue(parser, compiler, upvalueIndex, false);
    }

    return -1;
}

static int resolveLocal(Compiler* compiler, Token identifier) {
    for (int i = compiler->localCount - 1; i >= 0; i--) {
        Local local = compiler->locals[i];
        if (identifiersEqual(local.identifier, identifier)) {
            return i + local.significantTemps;
        }
    }

    return -1;          /* No local found with the given name */
}

static void addLocal(Parser* parser, Token identifier) {
    if (parser->compiler->localCount >= LVAR_MAX - 1) {
                errorAt(parser, &identifier, "Cannot contain more than 255 local variables in 1 function");
        return;
    }

    for (int i = parser->compiler->localCount - 1; i >= 0; i--) {
        Local local = parser->compiler->locals[i];

        if (local.depth < parser->compiler->scopeDepth) break;
        if (identifiersEqual(local.identifier, identifier)) {
            errorAt(parser, &identifier, "Attempt to re-declare a local variable inside the same scope");
            return;
        }

    }


    Local local;
    local.depth = parser->compiler->scopeDepth;
    local.identifier = identifier;
    local.significantTemps = parser->compiler->significantTemps;
    local.isCaptured = false;

    parser->compiler->locals[parser->compiler->localCount] = local;
    parser->compiler->localCount++;

    /* No opcode to define a local because this stack slot where we pushed the value 
     * is the local, the stack slot is reserved for it and the compiler takes account of 
     * which slot each local belongs to using the locals array*/

}

static bool identifiersEqual(Token id1, Token id2) {
    if (id1.length != id2.length) return false;
    return memcmp(id1.start, id2.start, id1.length) == 0;
}

static void parseLocalDeclaration(Scanner* scanner, Parser* parser) {
    advance(scanner, parser);               // Consume 'var' 
    consume(scanner, parser, TOKEN_IDENTIFIER, "Expected Identifier in local declaration");
    Token identifier = parser->previous;

    if (match(scanner, parser, TOKEN_EQUAL)) {
        expression(scanner, parser);
    } else {
        emitByte(parser, OP_NIL);
    }

    match(scanner, parser, TOKEN_SEMICOLON);
    
    addLocal(parser, identifier);
}

static int parseParameters(Scanner* scanner, Parser* parser, bool* variadicFlag) {
    consume(scanner, parser, TOKEN_ROUND_OPEN, "Expected Identifier in function declaration");
    int arity = 0;
    if (parser->current.type == TOKEN_IDENTIFIER) {
        do {
            consume(scanner, parser, TOKEN_IDENTIFIER, "Expected Identifier while parsing parameters");
            Token id = parser->previous;

            if (memcmp(id.start, "self", 4) == 0 || 
                    memcmp(id.start, "super", 5) == 0) {
                error(parser, "Parameter cannot be named 'self/super' in method");
            }

            if (parser->current.type == TOKEN_VAR_ARGS) {
                advance(scanner, parser);
                *variadicFlag = true;
                    
                if (parser->current.type != TOKEN_ROUND_CLOSE) {
                    error(parser, "No more parameters can proceed a variadic argument");
                    return 0;
                }
                    
                addLocal(parser, id);
                break;
            }
            addLocal(parser, id);
            arity++;
        } while (match(scanner, parser, TOKEN_COMMA));
    }

    consume(scanner, parser, TOKEN_ROUND_CLOSE, "Expected ')' while parsing parameters");
    
    if (arity > LVAR_MAX - 1) {
        error(parser, "Maximum parameter count reached for functions, cannot exceed 255");
    }

    return arity;
}

/* How 'self' and 'super' are passed:
 *
 * They are always added after all the parameters have been parsed, including 
 * variadic function types, sometimes we have special function cases like the 
 * class initialiser which has to discard its own return value in favour of returning 
 * the class object itself or `self`, in that case we set the function type to 
 * TYPE_INIT, which when is encountered while returns are parsed, it makes sure it always 
 * returns 'self' by fetching it's stack index accordingly */


static void parseClosureFunction(Scanner* scanner, Parser* parser, Token identifier, FunctionType type) {
    ObjFunction* function = newFunctionFromSource(parser->vm, identifier.start, identifier.length, 0);
    Compiler compiler;
    initCompiler(&compiler, function, type);
    setCompiler(&compiler, parser);
    beginScope(parser);
    bool variadic = false; 
    int arity = parseParameters(scanner, parser, &variadic);
    
    function->variadic = variadic;
    function->arity = arity;
    consume(scanner, parser, TOKEN_COLON, "Expected ':' in function declaration");
    
    while (parser->current.type != TOKEN_EOF &&
            parser->current.type != TOKEN_END) {
        statement(scanner, parser);
    }
    consume(scanner, parser, TOKEN_END, "Expected an 'end' to close function declaration");
    
    endCompiler(&compiler, parser, OP_RET);
    int constant = makeConstant(currentChunk(parser), OBJ(function));
    emitLongOperand(parser, (uint16_t)constant, OP_CLOSURE, OP_CLOSURE);
    emitClosureEncoding(parser, &compiler);

}

static void parseFunctionDeclaration(Scanner* scanner, Parser* parser) {
    advance(scanner, parser);               // Consume the 'func' 
    consume(scanner, parser, TOKEN_IDENTIFIER, "Expected Identifier in function declaration");
    Token identifier = parser->previous;

    parseClosureFunction(scanner, parser, identifier, TYPE_NORMAL);

    addLocal(parser, identifier);
}

static void parseFieldDeclaration(Scanner* scanner, Parser* parser, bool inherits) {
    advance(scanner, parser);
    Token identifier = parser->previous;

    consume(scanner, parser, TOKEN_EQUAL, "Expected an '=' in field declaration");
    int index = makeConstant(currentChunk(parser), 
            OBJ(allocateString(parser->vm, identifier.start, identifier.length)));

    expression(scanner, parser);
    emitLongOperand(parser, (uint16_t)index, OP_SET_CLASS_FIELD, OP_SET_CLASS_FIELD_LONG);
    emitByte(parser, inherits ? 1 : 0);
}

static void parseMethodDeclaration(Scanner* scanner, Parser* parser, bool inherits) {
    advance(scanner, parser);
    consume(scanner, parser, TOKEN_IDENTIFIER, "Expected identifier in method declaration");
    Token identifier = parser->previous;
    FunctionType type = TYPE_METHOD;

    if (identifier.length == 5 && memcmp(identifier.start, "_init", identifier.length) == 0) {
        type = TYPE_INIT;    
    }
    parseClosureFunction(scanner, parser, identifier, type);

    emitByte(parser, OP_METHOD);
    emitByte(parser, inherits ? 1 : 0);
}

static void parseClassStatement(Scanner* scanner, Parser* parser) {
    advance(scanner, parser);
    consume(scanner, parser, TOKEN_IDENTIFIER, "Expected Identifier in class declaration");
    Token identifier = parser->previous;
    Token superId;
    bool inherits = false;
    
    parseIdentifier(parser, identifier, OP_CLASS, OP_CLASS_LONG);
    addLocal(parser, identifier);
    
    if (match(scanner, parser, TOKEN_INHERITS)) {
        consume(scanner, parser, TOKEN_IDENTIFIER, "Expected an identifier for superclass");
        superId = parser->previous;

        parseReadIdentifier(scanner, parser, superId);
        
        beginScope(parser);
        addLocal(parser, token_super);
        emitByte(parser, OP_INHERIT);
        inherits = true;
    }

    consume(scanner, parser, TOKEN_COLON, "Expected a ':' in class declaration");


    while (parser->current.type != TOKEN_END &&
            parser->current.type != TOKEN_EOF) {
        switch (parser->current.type) {
            case TOKEN_IDENTIFIER:
                parseFieldDeclaration(scanner, parser, inherits);
                break;
            case TOKEN_FUNC:
                parseMethodDeclaration(scanner, parser, inherits);
                break;
            default: 
                errorAtCurrent(parser, "Unexpected token in class declaration");
                return;
        }
    }

    if (inherits) {
        endScope(parser);
    }

    consume(scanner, parser, TOKEN_END, "Expected 'end' in class declaration");
}

static void parseReturnStatement(Scanner* scanner, Parser* parser) {
    advance(scanner, parser);           // consume the return; 
    int numReturns = 1;

    if (parser->compiler->enclosing == NULL) {
        // Returns cannot take place in the outermost scope 
        error(parser, "Cannot use return in the outermost scope");
        return;
    }

    if (checkPrimary(scanner, parser)) {
        expression(scanner, parser);
    } else {
        if (parser->compiler->functionType == TYPE_INIT) {
            emitBytes(parser, OP_GET_LOCAL, (uint8_t)resolveLocal(parser->compiler, token_self));
        } else {
            emitByte(parser, OP_NIL);
        }
    }
    
    emitBytes(parser, OP_RET, (uint8_t)numReturns);

    match(scanner, parser, TOKEN_SEMICOLON);
}

static void parseIfStatement(Scanner* scanner, Parser* parser) {
    UintArray patchIndexes;
    initUintArray(&patchIndexes);

    advance(scanner, parser);
    expression(scanner, parser);
    
    consume(scanner, parser, TOKEN_COLON, "Expected an ':' in if statement");
    
    beginScope(parser);

    bool elseIfFound = false;
    bool elseFound = false;
    unsigned int latestElseIfIndex = 0;
    unsigned int ifIndex = emitJump(parser, OP_JMP_FALSE);
     
    while (parser->current.type != TOKEN_EOF &&
          parser->current.type != TOKEN_ELSE && 
          parser->current.type != TOKEN_ELSEIF &&
          parser->current.type != TOKEN_END) {
        statement(scanner, parser);
    }
    writeUintArray(&patchIndexes, emitJump(parser, OP_JMP));

    if (parser->current.type == TOKEN_EOF) {
        errorAtCurrent(parser, "Unclosed If-Block");
        return;
    }


    
    while (parser->current.type == TOKEN_ELSEIF) {
        if (!elseIfFound) {
            patch(parser, ifIndex);
            elseIfFound = true;
        } else {
            patch(parser, latestElseIfIndex);
        }

        advance(scanner, parser);
        expression(scanner, parser);
        consume(scanner, parser, TOKEN_COLON, "Expected an ':' in else-if clause");
        latestElseIfIndex = emitJump(parser, OP_JMP_FALSE);

        while (parser->current.type != TOKEN_EOF &&
               parser->current.type != TOKEN_ELSEIF &&
               parser->current.type != TOKEN_ELSE &&
               parser->current.type != TOKEN_END) {
            statement(scanner, parser);
        }
        writeUintArray(&patchIndexes, emitJump(parser, OP_JMP));

    }

    if (parser->current.type == TOKEN_ELSE) {
        advance(scanner, parser);
        consume(scanner, parser, TOKEN_COLON, "Expected ':' in else clause");
        
        if (elseIfFound) {
            patch(parser, latestElseIfIndex);
        } else {
            patch(parser, ifIndex);
        }
        elseFound = true;

        while (parser->current.type != TOKEN_EOF &&
               parser->current.type != TOKEN_END) {
            
            statement(scanner, parser);
        }
    }

    if (parser->current.type == TOKEN_EOF) {
        errorAtCurrent(parser, "Unclosed If-Block");
        return;
    }

    consume(scanner, parser, TOKEN_END, "Expected an 'end'");  

    
    
    /* Patch all the OP_JMP indexes to the end point */ 
    for (int i = 0; i < patchIndexes.count; i++) {
        patch(parser, getUintArray(&patchIndexes, i));
    }
    endScope(parser);

    if (!elseIfFound && !elseFound) {
        /* patch the unpatched OP_JMP_FALSE */
        patch(parser, ifIndex);
    }


    freeUintArray(&patchIndexes);
}

static void parseWhileStatement(Scanner* scanner, Parser* parser) {
    /* Store the index of the jump */
    unsigned int index = currentChunk(parser)->elem_count - 1; 

    /* Store the old state of the arrays
     * And replace it with another one which gets patched */
    UintArray* oldBreakArray = parser->unpatchedBreaks;
    UintArray newBreakArray;
    parser->unpatchedBreaks = &newBreakArray;
    initUintArray(parser->unpatchedBreaks);

    /////////////////////////////////////////////
        
    advance(scanner, parser);
    expression(scanner, parser);
    consume(scanner, parser, TOKEN_COLON, "Expected in ':' in while statement");

    unsigned int falseIndex = emitJump(parser, OP_JMP_FALSE);
    
    beginScope(parser);
    while (parser->current.type != TOKEN_EOF &&
           parser->current.type != TOKEN_END) {
        statement(scanner, parser);
    }
     
    int vars = endScope(parser);
    emitJumpBack(parser, index);
    patch(parser, falseIndex);

    consume(scanner, parser, TOKEN_END, "Expected an 'end'");



    /* Restore the old state and patch all jumps */ 

    parser->unpatchedBreaks = oldBreakArray;

    for (unsigned int i = 0; i < newBreakArray.count; i++) {
        unsigned int index = getUintArray(&newBreakArray, i);
        patch(parser, index);
        patchBreak(parser, index, vars);
    }

    
    freeUintArray(&newBreakArray);
    ///////////////////////////////////////////////
}

static void parseBreak(Scanner* scanner, Parser* parser) {
    advance(scanner, parser);
    if (parser->unpatchedBreaks == NULL) {
        errorAtCurrent(parser, "Break outside loop environment");
        return;
    }
    
    emitBytes(parser, OP_POPN, 0xff);           // Loop will patch this // 
    writeUintArray(parser->unpatchedBreaks, emitJump(parser, OP_JMP));
    match(scanner, parser, TOKEN_SEMICOLON);
}

static void parseNumericFor(Scanner* scanner, Parser* parser, 
        Token indexIdentifier, UintArray* oldBreakArray) {
    /* 'start' sig temps should already be pushed, index identifier should
     * already be a valid local in the outer scope,
     * compile the stop and increment */ 
    
    advance(scanner, parser);           // Consume the ',' 
    expression(scanner, parser); 
    
    if (match(scanner, parser, TOKEN_COMMA)) {
        expression(scanner, parser);
    } else {
        emitByte(parser, OP_PLUS1);
    }

    parser->compiler->significantTemps += 3;        /* 'start', 'stop' and 'increment' */ 
    consume(scanner, parser, TOKEN_COLON, "Expected an ':' in numeric for loop"); 

    // For loop begins
    beginScope(parser);
    unsigned int forLoopTopIndex = currentChunk(parser)->elem_count - 1;
    emitBytes(parser, OP_ITERATE_NUM, (uint8_t)resolveLocal(parser->compiler, indexIdentifier));
    unsigned int forLoopJmpIndex = emitJump(parser, OP_JMP_FALSE);
    
    while (parser->current.type != TOKEN_END && parser->current.type != TOKEN_EOF) {
        statement(scanner, parser);
    }
    int vars = endScope(parser);
    emitJumpBack(parser, forLoopTopIndex);
    patch(parser, forLoopJmpIndex);
    parser->compiler->significantTemps -= 3;
    emitBytes(parser, OP_POPN, (uint8_t)3);
    endScope(parser);
    consume(scanner, parser, TOKEN_END, "Expected an 'end' to close numeric for loop");
    
    /* Restore the old state and patch all jumps */ 
    
    UintArray* newBreakArray = parser->unpatchedBreaks;
    parser->unpatchedBreaks = oldBreakArray;

    for (unsigned int i = 0; i < newBreakArray->count; i++) {
        unsigned int index = getUintArray(newBreakArray, i);
        patch(parser, index);
        patchBreak(parser, index, vars);
    }

    
    freeUintArray(newBreakArray);
    ///////////////////////////////////////////////

}

static void parseForStatement(Scanner* scanner, Parser* parser) {
    beginScope(parser);
    advance(scanner, parser);       // Consume the 'for' 
    consume(scanner, parser, TOKEN_IDENTIFIER, "Expected Index Identifier in for loop");
    Token indexIdentifier = parser->previous;
    Token valueIdentifier;
    bool foundValueId = false; 
    
    /* Store the old state of the arrays
     * And replace it with another one which gets patched */
    UintArray* oldBreakArray = parser->unpatchedBreaks;
    UintArray newBreakArray;
    parser->unpatchedBreaks = &newBreakArray;
    initUintArray(parser->unpatchedBreaks);

    /////////////////////////////////////////////


    emitByte(parser, OP_NIL);
    addLocal(parser, indexIdentifier);
    if (match(scanner, parser, TOKEN_COMMA)) {
        consume(scanner, parser, TOKEN_IDENTIFIER, "Expected Value Identifier in for loop");
        foundValueId = true;
        valueIdentifier = parser->previous;
        emitByte(parser, OP_NIL);
        addLocal(parser, valueIdentifier);
    }

    consume(scanner, parser, TOKEN_IN, "Expected 'in' in for loop");
    expression(scanner, parser);        // Array              (sig temp)

    if (parser->current.type == TOKEN_COMMA && !foundValueId) {
        parseNumericFor(scanner, parser, indexIdentifier, oldBreakArray);
        return;
    } 

    emitByte(parser, OP_MIN1);          // Index value holder (sig temp)
    parser->compiler->significantTemps += 2;

    consume(scanner, parser, TOKEN_COLON, "Expected an ':' in for loop");
    beginScope(parser);
    // For loop starts 
    unsigned int forLoopTopIndex = currentChunk(parser)->elem_count - 1;
    
    if (foundValueId) {
        emitByte(parser, OP_ITERATE_VALUE);
        emitBytes(parser, 
                 (uint8_t)resolveLocal(parser->compiler, indexIdentifier), 
                 (uint8_t)resolveLocal(parser->compiler, valueIdentifier)
        );
    } else {
        emitByte(parser, OP_ITERATE);
        emitByte(parser,
                 (uint8_t)resolveLocal(parser->compiler, indexIdentifier)
        );
    }

    unsigned int forLoopTopJmp = emitJump(parser, OP_JMP_FALSE);
    while (parser->current.type != TOKEN_EOF &&
           parser->current.type != TOKEN_END) {
        
        statement(scanner, parser);
    }
    int vars = endScope(parser);
    emitJumpBack(parser, forLoopTopIndex);

    patch(parser, forLoopTopJmp);
    emitBytes(parser, OP_POPN, (uint8_t)2);           // pop the array and indexholder
    parser->compiler->significantTemps -= 2;
    endScope(parser);

    /* Restore the old state and patch all jumps */ 

    parser->unpatchedBreaks = oldBreakArray;

    for (unsigned int i = 0; i < newBreakArray.count; i++) {
        unsigned int index = getUintArray(&newBreakArray, i);
        patch(parser, index);
        patchBreak(parser, index, vars);
    }

    
    freeUintArray(&newBreakArray);
    ///////////////////////////////////////////////


    consume(scanner, parser, TOKEN_END, "Expected an 'end' to close for loop");
}

static void synchronize(Scanner* scanner, Parser* parser) {
    while (parser->current.type != TOKEN_EOF) {
        advance(scanner, parser);
        switch (parser->current.type) {
            case TOKEN_VAR:
            case TOKEN_CLASS:
            case TOKEN_GLOBAL:
            case TOKEN_FUNC:
            case TOKEN_RETURN:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_FOR:
            case TOKEN_BREAK:
                parser->panicMode = true;
                return;

            default: ;
        }
    }

    if (parser->current.type != TOKEN_EOF) {
        parser->panicMode = false;
    }
}

static void statement(Scanner* scanner, Parser* parser) {
    switch (parser->current.type) {
        case TOKEN_GLOBAL: parseGlobalDeclaration(scanner, parser); break;
        case TOKEN_VAR: parseLocalDeclaration(scanner, parser); break;
        case TOKEN_FUNC: parseFunctionDeclaration(scanner, parser); break;
        case TOKEN_IF: parseIfStatement(scanner, parser); break;
        case TOKEN_WHILE: parseWhileStatement(scanner, parser); break;
        case TOKEN_BREAK: parseBreak(scanner, parser); break;
        case TOKEN_IDENTIFIER: callStatement(scanner, parser); break;
        case TOKEN_SELF: callStatement(scanner, parser); break;
        case TOKEN_SUPER: callStatement(scanner, parser); break;
        case TOKEN_FOR: parseForStatement(scanner, parser); break;
        case TOKEN_CLASS: parseClassStatement(scanner, parser); break;
        case TOKEN_RETURN: parseReturnStatement(scanner, parser); break;
        default: errorAtCurrent(parser, "Unexpected Token, expected a statement (Syntax Error)");
    }
    if (parser->panicMode) synchronize(scanner, parser);
}

static void parseSupercall(Scanner* scanner, Parser* parser, bool shouldReturn) {
    consume(scanner, parser, TOKEN_DOT, "Expected an index call expression");
    consume(scanner, parser, TOKEN_IDENTIFIER, "Expected identifier during super call");

    Token identifier = parser->previous;
    parseIdentifier(parser, identifier, OP_CONST, OP_CONST_LONG);    
    bool function = false;
    int argCount = 0;

    if (parser->current.type == TOKEN_ROUND_OPEN) {
        argCount = parseFunctionArguments(scanner, parser);
        function = true;
    }

    
    bool foundSelf = parseReadIdentifier(scanner, parser, token_self);
    bool foundSuper = parseReadIdentifier(scanner, parser, token_super);

    if (!foundSelf) {
        error(parser, "Supercall outside method");
        return;
    } 

    if (!foundSuper) {
        error(parser, "Class doesn't have any superclass");
        return;
    }

    if (function) {
        emitByte(parser, OP_SUPERCALL);
        emitBytes(parser, (uint8_t)argCount, shouldReturn ? 1 : 0);
    } else {
        emitByte(parser, OP_GET_SUPER);
    }
}

static void callStatement(Scanner* scanner, Parser* parser) {
    /* this function's job is to decide whether the statement is a suitable
     * identifier call statement or not, if its not, it calls the assignmentStatement()
     * function to continue with assignment */
    Token identifier = parser->current;
    advance(scanner, parser);
    int type = CALL_NONE;
 
    if (identifiersEqual(identifier, token_super)) {
        parseSupercall(scanner, parser, false);
        type = parseCallSequenceField(scanner, parser, false);

        if (type != CALL_FUNC) {
            error(parser, "Expected a supercall");
            return;
        }
        match(scanner, parser, TOKEN_SEMICOLON);
        return;
    }

    if (checkCall(scanner, parser)) {
        /* Call sequences guranteed 
         * since standalone identifiers are compiled by the assignment functions, while 
         * indexing and calling need to evaluate that identifier into a value before 
         * continuing, we parse it right before starting to parse the call / indexing*/
        parseReadIdentifier(scanner, parser, identifier);
        type = parseCallSequenceField(scanner, parser, false); // 0 return values expected
        if (type == CALL_FUNC) {
            /* do function / class calling stuff 
             * Not a valid assignment target 
             * (we already parsed the function call)*/ 
            match(scanner, parser, TOKEN_SEMICOLON);
            return;
        }
    }
    
    /* Valid assignment target */
    assignmentStatement(scanner, parser, type);

}

static void assignmentStatement(Scanner* scanner, Parser* parser, int type) {
    // first value has already been parsed unless its a raw  identifier 
    Token id = parser->previous;
    // we save the operator and parse the value to be assigned 

    Token operator = parser->current;
    advance(scanner, parser);

    expression(scanner, parser);
    
    int global1 = -1;
    int global2 = -1;
    int local1 = -1;
    int customIndex1 = -1;
    int upvalue1 = -1;
    int field1 = -1;

    switch (operator.type) {
        case TOKEN_EQUAL:
            local1 = OP_ASSIGN_LOCAL;
            upvalue1 = OP_ASSIGN_UPVALUE;
            global1 = OP_ASSIGN_GLOBAL;
            global2 = OP_ASSIGN_LONG_GLOBAL;
            customIndex1 = OP_CUSTOM_INDEX_MOD;
            field1 = OP_SET_FIELD;
            break;
        case TOKEN_PLUS_EQUAL:
            local1 = OP_PLUS_ASSIGN_LOCAL;
            upvalue1 = OP_PLUS_ASSIGN_UPVALUE;
            global1 = OP_PLUS_ASSIGN_GLOBAL;
            global2 = OP_PLUS_ASSIGN_LONG_GLOBAL;
            customIndex1 = OP_CUSTOM_INDEX_PLUS_MOD;
            break;
        case TOKEN_MINUS_EQUAL:
            local1 = OP_MINUS_ASSIGN_LOCAL;
            upvalue1 = OP_MINUS_ASSIGN_UPVALUE;
            global1 = OP_SUB_ASSIGN_GLOBAL;
            global2 = OP_SUB_ASSIGN_LONG_GLOBAL;
            customIndex1 = OP_CUSTOM_INDEX_SUB_MOD;
            break;
        case TOKEN_MUL_EQUAL:
            local1 = OP_MUL_ASSIGN_LOCAL;
            upvalue1 = OP_MUL_ASSIGN_UPVALUE;
            global1 = OP_MUL_ASSIGN_GLOBAL;
            global2 = OP_MUL_ASSIGN_LONG_GLOBAL;
            customIndex1 = OP_CUSTOM_INDEX_MUL_MOD;
            break;
        case TOKEN_DIV_EQUAL:
            local1 = OP_DIV_ASSIGN_LOCAL;
            upvalue1 = OP_DIV_ASSIGN_UPVALUE;
            global1 = OP_DIV_ASSIGN_GLOBAL;
            global2 = OP_DIV_ASSIGN_LONG_GLOBAL;
            customIndex1 = OP_CUSTOM_INDEX_DIV_MOD;
            break;
        case TOKEN_POW_EQUAL:
            local1 = OP_POW_ASSIGN_LOCAL;
            upvalue1 = OP_POW_ASSIGN_UPVALUE;
            global1 = OP_POW_ASSIGN_GLOBAL;
            global2 = OP_POW_ASSIGN_LONG_GLOBAL;
            customIndex1 = OP_CUSTOM_INDEX_POW_MOD;
            break;
        default:
            errorAt(parser, &operator, "Expected assignment operator");
            return;
    }

    if (type == CALL_NONE) {
        int localIndex = resolveLocal(parser->compiler, id);

        if (localIndex != -1) {
            emitBytes(parser, (uint8_t)local1, (uint8_t)localIndex);
        } else {
            int upvalueIndex = resolveUpvalue(parser, parser->compiler, id);

            if (upvalueIndex != -1) {
                emitBytes(parser, (uint8_t)upvalue1, (uint8_t)upvalueIndex);
            } else {
                parseIdentifier(parser, id, (uint8_t)global1, (uint8_t)global2); 
            }
        }
    } else if (type == CALL_ARRAY) {
        emitByte(parser, (uint8_t)customIndex1);
    } else if (type == CALL_DOT) {
        emitByte(parser, (uint8_t)field1);
    }

    match(scanner, parser, TOKEN_SEMICOLON);
}

void endCompiler(Compiler* compiler, Parser* parser, uint8_t ins) {
    if (ins == OP_RET) {
        if (parser->compiler->functionType == TYPE_INIT) {
            emitBytes(parser, OP_GET_LOCAL, (uint8_t)resolveLocal(parser->compiler, token_self));
        } else {
            emitByte(parser, OP_NIL);
        }

        // return nil by default unless theres another return above this one 
        emitByte(parser, ins);
        emitByte(parser, 1);                // all user-defined functions have atleast 1 return 
                                            // value expected 
    } else {
        emitByte(parser, ins);
    }

    if (parser->compiler->enclosing == NULL) return;
    parser->compiler = parser->compiler->enclosing;
}

InterpretResult compile(const char* source, VM* vm, ObjFunction* function) {
    Scanner scanner;
    Parser parser;
    Compiler compiler;

    // Init parser already sets the compiler for us

    initScanner(&scanner, source);
    initCompiler(&compiler, function, TYPE_NORMAL);
    initParser(&parser, &compiler, vm);

    advance(&scanner, &parser);
    beginScope(&parser);
    while (!match(&scanner, &parser, TOKEN_EOF)) {
        statement(&scanner, &parser);
    }
    
    // endScope(&parser);           // RETEOF resets the stack automatically
    match(&scanner, &parser, TOKEN_EOF);

    endCompiler(&compiler, &parser, OP_RETEOF);
    #ifdef DEBUG_PRINT_BYTECODE
    
    if (!parser.hadError) {
        dissembleChunk(0, currentChunk(&parser), function->name->allocated);
    }
    #endif
    /* 
    int line = -1;
    

    
    for (;;) {
        Token token = scanToken(&scanner);
        if (token.line != line) {
            printf("%4d ", token.line);
            line = token.line;
        } else {
            printf("   |");
        }

        printf("%2d '%.*s'\n", token.type, token.length, token.start);

        if (token.type == TOKEN_EOF) break;
    }
    */

    return !parser.hadError ? INTERPRET_OK : INTERPRET_COMPILE_ERROR;
}
