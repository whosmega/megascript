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

static void assignmentStatement(Scanner* scanner, Parser* parser);
static void callStatement(Scanner* scanner, Parser* parser);
static void statement(Scanner* scanner, Parser* parser);
static void expression(Scanner* scanner, Parser* parser);
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
static void parseReadIdentifier(Scanner* scanner, Parser* parser);
static void parseGrouping(Scanner* scanner, Parser* parser);
static void parseGlobalDeclaration(Scanner* scanner, Parser* parser); 
static void parseVariableDeclaration(Scanner* scanner, Parser* parser);
static void parseIdentifier(Parser* parser, Token identifier, uint8_t normins, uint8_t longins);
static void parseAssign(Scanner* scanner, Parser* parser, Token identifier);
static void parsePlusAssign(Scanner* scanner, Parser* parser, Token identifier);
static void parseMinusAssign(Scanner* scanner, Parser* parser, Token identifier);
static void parseMulAssign(Scanner* scanner, Parser* parser, Token identifier);
static void parseDivAssign(Scanner* scanner, Parser* parser, Token identifier);
static void parsePowAssign(Scanner* scanner, Parser* parser, Token identifier);
static void parseBlock(Scanner* scanner, Parser* parser);
static void parseLocalDeclaration(Scanner* scanner, Parser* parser);
static bool identifiersEqual(Token id1, Token id2);
static int resolveLocal(Parser* parser, Token identifier);
static void parsePrintStatement(Scanner* scanner, Parser* parser);

void initCompiler(Compiler* compiler) { 
    compiler->localCount = 0;
    compiler->scopeDepth = 0;
}

void initParser(Parser* parser, Chunk* chunk, Compiler* compiler, VM* vm) {
    parser->hadError = false;
    parser->panicMode = false;
    parser->vm = vm;
    parser->compilingChunk = chunk;
    parser->compiler = compiler;
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
    if (parser->panicMode == true) return;      /* Suppress */
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
    return parser->compilingChunk;
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

static void checkBadExpression(Scanner* scanner, Parser* parser) {
    /* If next token is a primary after a full operator
     * sequence, report an error */
    if (checkPrimary(scanner, parser)) {
        errorAtCurrent(parser, "Unexpected literal in operator sequence (Syntax Error)");
        return;
    }
}


static void expression(Scanner* scanner, Parser* parser) {
    if (!checkPrimary(scanner, parser)) return;         /* If we dont find a primary expression, 
                                                           we arent going to be parsing any significant ex */
    equality(scanner, parser);
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
        match(scanner, parser, TOKEN_MINUS)) {
        TokenType type = parser->previous.type;
        call(scanner, parser);
        switch (type) {
            case TOKEN_BANG:
                emitByte(parser, OP_NOT);
                break;
            case TOKEN_MINUS:
                emitByte(parser, OP_NEGATE);
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

static void call(Scanner* scanner, Parser* parser) {
    primary(scanner, parser);
}

static void primary(Scanner* scanner, Parser* parser) {
    if (!checkPrimary(scanner, parser)) {
        errorAtCurrent(parser, "Expected Expression (Syntax Error)");
        return;
    }
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
            parseReadIdentifier(scanner, parser); 
            break;
        default: return;
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
        case TOKEN_ROUND_OPEN:
        /* unaries */
        case TOKEN_MINUS:
        case TOKEN_BANG:
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
    Value value = OBJ(allocateString(parser->vm, parser->previous.start + 1, parser->previous.length - 2));
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

static void parseIdentifier(Parser* parser, Token identifier, uint8_t normins, uint8_t longins) {
    Value value = OBJ(allocateString(parser->vm, identifier.start, identifier.length));
    uint16_t index = makeConstant(currentChunk(parser), value);

    emitLongOperand(parser, index, normins, longins);
}

static void parseReadIdentifier(Scanner* scanner, Parser* parser) {
    advance(scanner, parser);           // Consume the identifier

    Token identifier = parser->previous;
    int localIndex = resolveLocal(parser, identifier);

    if (localIndex == -1) {
        parseIdentifier(parser, identifier, OP_GET_GLOBAL, OP_GET_LONG_GLOBAL);
    } else {
        emitBytes(parser, OP_GET_LOCAL, (uint8_t)localIndex);
    }
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

static void parseAssign(Scanner* scanner, Parser* parser, Token identifier) { 
    advance(scanner, parser);
    expression(scanner, parser);
    
    int localIndex = resolveLocal(parser, identifier);
    if (localIndex == -1) {
        parseIdentifier(parser, identifier, OP_ASSIGN_GLOBAL, OP_ASSIGN_LONG_GLOBAL);
    } else {
        emitBytes(parser, OP_ASSIGN_LOCAL, (uint8_t)localIndex);
    }
}

static void parsePlusAssign(Scanner* scanner, Parser* parser, Token identifier) {
    advance(scanner, parser);           // Consume operator 
    expression(scanner, parser);

    int localIndex = resolveLocal(parser, identifier);

    if (localIndex == -1) {
        parseIdentifier(parser, identifier, OP_PLUS_ASSIGN_GLOBAL, OP_PLUS_ASSIGN_LONG_GLOBAL);
    } else {
        emitBytes(parser, OP_PLUS_ASSIGN_LOCAL, (uint8_t)localIndex);
    }
}

static void parseMinusAssign(Scanner* scanner, Parser* parser, Token identifier) {
    advance(scanner, parser);
    expression(scanner, parser);

    int localIndex = resolveLocal(parser, identifier);

    if (localIndex == -1) {
        parseIdentifier(parser, identifier, OP_SUB_ASSIGN_GLOBAL, OP_SUB_ASSIGN_LONG_GLOBAL);
    } else {
        emitBytes(parser, OP_MINUS_ASSIGN_LOCAL, (uint8_t)localIndex);
    }
}

static void parseMulAssign(Scanner* scanner, Parser* parser, Token identifier) {
    advance(scanner, parser);
    expression(scanner, parser);

    int localIndex = resolveLocal(parser, identifier);

    if (localIndex == -1) {
        parseIdentifier(parser, identifier, OP_MUL_ASSIGN_GLOBAL, OP_MUL_ASSIGN_LONG_GLOBAL);
    } else {
        emitBytes(parser, OP_MUL_ASSIGN_LOCAL, (uint8_t)localIndex);
    }
}

static void parseDivAssign(Scanner* scanner, Parser* parser, Token identifier) {
    advance(scanner, parser);
    expression(scanner, parser);

    int localIndex = resolveLocal(parser, identifier);

    if (localIndex == -1) {
        parseIdentifier(parser, identifier, OP_DIV_ASSIGN_GLOBAL, OP_DIV_ASSIGN_LONG_GLOBAL);
    } else {
        emitBytes(parser, OP_DIV_ASSIGN_LOCAL, (uint8_t)localIndex);
    }
}

static void parsePowAssign(Scanner* scanner, Parser* parser, Token identifier) {
    advance(scanner, parser);
    expression(scanner, parser);

    int localIndex = resolveLocal(parser, identifier);

    if (localIndex == -1) {
        parseIdentifier(parser, identifier, OP_POW_ASSIGN_GLOBAL, OP_POW_ASSIGN_LONG_GLOBAL);
    } else {
        emitBytes(parser, OP_POW_ASSIGN_LOCAL, (uint8_t)localIndex);
    }
}

static void beginScope(Parser* parser) {
    parser->compiler->scopeDepth++;
}

static void endScope(Parser* parser) {
    int count = 0;
    for (int i = parser->compiler->localCount - 1; i >= 0; i--) {
        if (parser->compiler->locals[i].depth != parser->compiler->scopeDepth) break;
        count++;
        parser->compiler->localCount--;
    }

    if (count != 0) {
        emitBytes(parser, OP_POPN, (uint8_t)count);         // Pop off all the locals which were made
    }
    parser->compiler->scopeDepth--;

}

static int resolveLocal(Parser* parser, Token identifier) {
    for (int i = parser->compiler->localCount - 1; i >= 0; i--) {
        Local local = parser->compiler->locals[i];
        if (identifiersEqual(local.identifier, identifier)) {
            return i;
        }
    }

    return -1;          /* No local found with the given name */
}

static bool identifiersEqual(Token id1, Token id2) {
    if (id1.length != id2.length) return false;
    return memcmp(id1.start, id2.start, id1.length) == 0;
}

static void parseLocalDeclaration(Scanner* scanner, Parser* parser) {
    advance(scanner, parser);               // Consume 'var' 
    consume(scanner, parser, TOKEN_IDENTIFIER, "Expected Identifier in local declaration");
    Token identifier = parser->previous;

    if (parser->compiler->localCount >= UINT8_MAX + 1) {
        errorAt(parser, &identifier, "Cannot contain more than 256 local variables in 1 scope");
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

    if (match(scanner, parser, TOKEN_EQUAL)) {
        expression(scanner, parser);
    } else {
        emitByte(parser, OP_NIL);
    }

    match(scanner, parser, TOKEN_SEMICOLON);

    Local local;
    local.depth = parser->compiler->scopeDepth;
    local.identifier = identifier;

    parser->compiler->locals[parser->compiler->localCount] = local;
    parser->compiler->localCount++;

    /* No opcode to define a local because this stack slot where we pushed the value 
     * is the local, the stack slot is reserved for it and the compiler takes account of 
     * which slot each local belongs to using the locals array*/
}

static void parsePrintStatement(Scanner* scanner, Parser* parser) {
    advance(scanner, parser);
    expression(scanner, parser);
    emitByte(parser, OP_PRINT);
    match(scanner, parser, TOKEN_SEMICOLON);
}

static unsigned int emitJump(Parser* parser, uint8_t ins1, uint8_t ins2) {
    unsigned int index = currentChunk(parser)->elem_count + 1;
    
    if ((index - 1) <= UINT8_MAX) {
        emitBytes(parser, ins1, 0xff);
    } else {
        emitLongOperand(parser, 0xffff, ins2, ins2);
    }

    return index;
}

static void patch(Parser* parser, unsigned int index) {
    uint8_t instruction = currentChunk(parser)->code[index - 1];
    /* Minus one because instructions are base 0, while the actual count is base 1 */
    uint16_t offset = currentChunk(parser)->elem_count - index - 1;

    if (instruction == OP_JMP || instruction == OP_JMP_FALSE || instruction == OP_JMP_BACK) {
        /* 8 bit */ 
        currentChunk(parser)->code[index] = (uint8_t)offset;
    } else if (instruction == OP_JMP_LONG || instruction == OP_JMP_FALSE_LONG || instruction == OP_JMP_BACK_LONG) {
       writeLongByte(currentChunk(parser), offset, parser->previous.line); 
    } else {
    }
}

static void parseIfStatement(Scanner* scanner, Parser* parser) {
    UintArray patchIndexes;
    initUintArray(&patchIndexes);

    advance(scanner, parser);
    expression(scanner, parser);
    
    if (parser->current.type != TOKEN_COLON) {
        errorAtCurrent(parser, "Expected an ':' in if statement");
        return;
    }

    advance(scanner, parser);
    
    beginScope(parser);

    bool elseIfFound = false;
    bool elseFound = false;
    unsigned int latestElseIfIndex = 0;
    unsigned int ifIndex = emitJump(parser, OP_JMP_FALSE, OP_JMP_FALSE_LONG);
     
    while (parser->current.type != TOKEN_EOF &&
          parser->current.type != TOKEN_ELSE && 
          parser->current.type != TOKEN_ELSEIF &&
          parser->current.type != TOKEN_END) {
        statement(scanner, parser);
    }
    writeUintArray(&patchIndexes, emitJump(parser, OP_JMP, OP_JMP_LONG));

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
        latestElseIfIndex = emitJump(parser, OP_JMP_FALSE, OP_JMP_FALSE_LONG);

        while (parser->current.type != TOKEN_EOF &&
               parser->current.type != TOKEN_ELSEIF &&
               parser->current.type != TOKEN_ELSE &&
               parser->current.type != TOKEN_END) {
            statement(scanner, parser);
        }
        writeUintArray(&patchIndexes, emitJump(parser, OP_JMP, OP_JMP_LONG));

    }

    if (parser->current.type == TOKEN_ELSE) {
        advance(scanner, parser);

        if (parser->current.type != TOKEN_COLON) {
            errorAtCurrent(parser, "Expected ':' in else clause");
        }
        advance(scanner, parser);
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
    
    if (!elseIfFound && !elseFound) {
        /* patch the unpatched OP_JMP_FALSE */
        patch(parser, ifIndex);
    }

    /* Patch all the OP_JMP indexes to the end point */ 
    for (int i = 0; i < patchIndexes.count; i++) {
        patch(parser, getUintArray(&patchIndexes, i));
    }

    endScope(parser);
    freeUintArray(&patchIndexes);
}

static void parseBlock(Scanner* scanner, Parser* parser) {
    advance(scanner, parser);           // Parse the ':'

    /* Enter a new scope */
    beginScope(parser);

    while (parser->current.type != TOKEN_END &&
           parser->current.type != TOKEN_EOF) {
        
        statement(scanner, parser);
    }

    if (parser->current.type == TOKEN_EOF) {
        errorAtCurrent(parser, "Unclosed block");
        return;
    }

    consume(scanner, parser, TOKEN_END, "Expected an 'end' to close block");

    /* Leave the scope */ 
    endScope(parser);
}



static void synchronize(Scanner* scanner, Parser* parser) {
    parser->panicMode = false;
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
                return;

            default: ;
        }
    }
}

static void statement(Scanner* scanner, Parser* parser) {
    switch (parser->current.type) {
        case TOKEN_GLOBAL: parseGlobalDeclaration(scanner, parser); break;
        case TOKEN_VAR: parseLocalDeclaration(scanner, parser); break;
        case TOKEN_IF: parseIfStatement(scanner, parser); break;
        case TOKEN_PRINT: parsePrintStatement(scanner, parser); break;
        case TOKEN_IDENTIFIER: callStatement(scanner, parser); break;
        default: errorAtCurrent(parser, "Unexpected Token, expected a statement (Syntax Error)");
    }
    if (parser->panicMode) synchronize(scanner, parser);
}

static void callStatement(Scanner* scanner, Parser* parser) {
    /* this function's job is to decide whether the statement is a suitable 
     * identifier call statement or not, if its not, it calls the assignmentStatement() 
     * function to continue with assignment */
    assignmentStatement(scanner, parser);
}

static void assignmentStatement(Scanner* scanner, Parser* parser) {
    /* Temporarily assigns only single identifier globals */ 
    Token identifier = parser->current;
    advance(scanner, parser);                   // Consume the identifier
    
    switch (parser->current.type) {
        case TOKEN_EQUAL:
            parseAssign(scanner, parser, identifier);
            break;
        case TOKEN_PLUS_EQUAL:
            parsePlusAssign(scanner, parser, identifier);
            break;
        case TOKEN_MINUS_EQUAL:
            parseMinusAssign(scanner, parser, identifier);
            break;
        case TOKEN_MUL_EQUAL:
            parseMulAssign(scanner, parser, identifier);
            break;
        case TOKEN_DIV_EQUAL:
            parseDivAssign(scanner, parser, identifier);
            break;
        case TOKEN_POW_EQUAL:
            parsePowAssign(scanner, parser, identifier);
            break;
        default: 
            errorAtCurrent(parser, "Expected Assignment");
            break;
    }
    match(scanner, parser, TOKEN_SEMICOLON);
}

InterpretResult compile(const char* source, Chunk* chunk, VM* vm) {
    Scanner scanner;
    Parser parser;
    Compiler compiler;

    initScanner(&scanner, source);
    initCompiler(&compiler);
    initParser(&parser, chunk, &compiler, vm);
    advance(&scanner, &parser);
    
    beginScope(&parser);
    while (!match(&scanner, &parser, TOKEN_EOF)) {
        statement(&scanner, &parser);
    }
    endScope(&parser);

    emitByte(&parser, OP_EOF);

    #ifdef DEBUG_PRINT_BYTECODE
    
    if (!parser.hadError) {
        dissembleChunk(chunk, "MAIN");
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
