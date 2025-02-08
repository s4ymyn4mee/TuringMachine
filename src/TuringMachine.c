#include <fcntl.h>
#include <unistd.h>

#define BUFFER_SIZE 256
#define FILENAME_SIZE 256

#define MAX_HASHTABLE_SIZE 5003
#define MAX_STEP_AMOUNT 100000
#define MAX_STATE_AMOUNT 256

typedef struct Node {
    struct Node *previous;
    struct Node *next;
    char symbol;
} Node;

typedef struct LinkedTape {
    Node *head;
    Node *tail;
    unsigned int size;
} LinkedTape;

typedef struct MemoryBlock {
    void *ptr;
    size_t size;
    struct MemoryBlock *next;
} MemoryBlock;

typedef struct Transition {
    char currentSymbol;
    char currentState[BUFFER_SIZE];
    char newSymbol;
    char newState[BUFFER_SIZE];
    char direction; // символы направления >, <, .
} Transition;

typedef struct TransitionTable {
    Transition *table[MAX_HASHTABLE_SIZE];
} TransitionTable;


MemoryBlock *memoryList = 0;

TransitionTable transitionTable;
int transitionAmount = 0;

char states[MAX_STATE_AMOUNT][BUFFER_SIZE];
int stateAmount = 0;


/**
 * Функция для чтения строки до переноса строки
 */
long readline(int fileDescriptor, char *buffer, unsigned long size) {
    long readBytesAmount;
    unsigned long i = 0;

    while (i < size - 1) {
        readBytesAmount = read(fileDescriptor, &buffer[i], 1);
        if (readBytesAmount == 1) {
            if (buffer[i] == '\n') {
                buffer[i] = '\0';

                return i;
            }
            i++;
        } else if (readBytesAmount == 0) {
            break;
        } else {
            return -1;
        }
    }

    buffer[i] = '\0';  // В случае, если не достигли размера буфера

    return i;
}

/**
 * Функция для чтения количества состояний и состояний
 */
void readStates(int fileDescriptor) {
    char buffer[BUFFER_SIZE];
    long readBytesAmount = readline(fileDescriptor, buffer, BUFFER_SIZE - 1);

    if (readBytesAmount <= 0) {
        write(STDERR_FILENO, "Error reading the number of states!\n", 37);
        return;
    }
    if (buffer[readBytesAmount - 1] == '\n') {
        buffer[readBytesAmount - 1] = '\0';
    } else {
        buffer[readBytesAmount] = '\0';
    }

    stateAmount = 0;
    for (int i = 0; i < readBytesAmount; i++) {
        if (buffer[i] >= '0' && buffer[i] <= '9') {
            stateAmount = stateAmount * 10 + buffer[i] - '0';
        } else {
            write(STDERR_FILENO, "Invalid state count format!\n", 29);
            return;
        }
    }

    readBytesAmount = readline(fileDescriptor, buffer, BUFFER_SIZE - 1);
    if (readBytesAmount <= 0) {
        write(STDERR_FILENO, "Error reading the states line!\n", 32);
        return;
    }
    if (buffer[readBytesAmount - 1] == '\n') {
        buffer[readBytesAmount - 1] = '\0';
    } else {
        buffer[readBytesAmount] = '\0';
    }

    int i = 0;
    int j = 0;
    int k = 0;
    while (buffer[i] != '\0') {
        if (buffer[i] == ' ') {
            states[k][j] = '\0';
            k++;
            j = 0;

            while (buffer[i] == ' ') {
                i++;
            }
        } else {
            states[k][j] = buffer[i];
            j++;
            i++;
        }
    }

    if (j > 0) {
        states[k][j] = '\0';
    }
}


/**
 * Функция хеширования ключей для хеш-таблицы
 */
unsigned int hash(char currentSymbol, char *currentState) {
    unsigned int hashValue = currentSymbol;
    for (int i = 0; currentState[i] != '\0'; i++) {
        hashValue = 53 * hashValue + currentState[i];
    }

    return hashValue % MAX_HASHTABLE_SIZE;
}


/**
 * Функция для поиска перехода в таблице переходов
 */
Transition* findTransition(char currentSymbol, char *currentState) {
    unsigned int index = hash(currentSymbol, currentState);
    unsigned int startIndex = index;

    while (transitionTable.table[index] != 0) {
        Transition *transition = transitionTable.table[index];
        int match = 1;

        if (transition->currentSymbol != currentSymbol) {
            match = 0;
        } else {
            int i = 0;
            while (currentState[i] != '\0' && transition->currentState[i] != '\0') {
                if (currentState[i] != transition->currentState[i]) {
                    match = 0;
                    break;
                }
                i++;
            }
            if (currentState[i] != '\0' || transition->currentState[i] != '\0') {
                match = 0;
            }
        }

        if (match) {
            return transition;
        }

        index = (index + 1) % MAX_HASHTABLE_SIZE;
        if (index == startIndex) {
            break;
        }
    }

    return 0;
}


/**
 * Функция для вставки перехода в таблицу переходов
 */
void insertTransition(Transition *transition) {
    unsigned int index = hash(transition->currentSymbol, transition->currentState);

    while (transitionTable.table[index] != 0) {
        index = (index + 1) % MAX_HASHTABLE_SIZE;
    }
    transitionTable.table[index] = transition;
}


/**
 * Функция выделения блока памяти
 */
void* allocateMemory(unsigned long size) {
    void *ptr = sbrk(size);

    if (ptr == (void*)-1) {
        write(STDERR_FILENO, "Memory allocation failed!\n", 27);

        return 0;
    }

    MemoryBlock *newBlock = (MemoryBlock*)sbrk(sizeof(MemoryBlock));
    if (newBlock == (void*)-1) {
        write(STDERR_FILENO, "Memory block allocation failed!\n", 33);

        return 0;
    }

    newBlock->ptr = ptr;
    newBlock->size = size;
    newBlock->next = memoryList;
    memoryList = newBlock;

    return ptr;
}


/**
 * Функция освобождения выделенного блока памяти
 */
void releaseMemory(void* ptr) {
    MemoryBlock *current = memoryList;
    MemoryBlock *previous = 0;

    while (current != 0) {
        if (current->ptr == ptr) {
            if (previous == 0) {
                memoryList = current->next;
            } else {
                previous->next = current->next;
            }

            if (brk(current->ptr) == -1) {
                write(STDERR_FILENO, "Memory deallocation failed!\n", 29);
            }

            if (brk(current) == -1) {
                write(STDERR_FILENO, "Failed to free MemoryBlock structure!\n", 39);
            }

            return;
        }

        previous = current;
        current = current->next;
    }

    write(STDERR_FILENO, "Pointer not found in allocated memory!\n", 40);
}


/**
 * Функция освобождения всех выделенных блоков памяти
 */
void releaseAllMemory() {
    MemoryBlock *current = memoryList;
    MemoryBlock *next = NULL;

    int p = 0;
    while (current != NULL) {
        next = current->next;

        if (brk(current->ptr) == -1) {
            write(STDERR_FILENO, "Failed to deallocate memory block!\n", 36);
        }

        if (brk(current) == -1) {
            write(STDERR_FILENO, "Failed to deallocate MemoryBlock structure!\n", 45);
        }

        current = next;
        p++;
    }
    
    memoryList = NULL;
}


/**
 * Функция для вывода состояния ленты на консоль
 */
void printTape(LinkedTape *tape) {
    char tempBuffer[2];
    tempBuffer[1] = '\0';

    Node *leftNode = tape->head;
    while (leftNode != NULL && (leftNode->symbol == '_' || leftNode->symbol == ' ')) {
        leftNode = leftNode->next;
    }

    Node *rightNode = tape->tail;
    while (rightNode != NULL && (rightNode->symbol == '_' || rightNode->symbol == ' ')) {
        rightNode = rightNode->previous;
    }

    if (leftNode == NULL || rightNode == NULL) {
        write(STDOUT_FILENO, "\n", 2);

        return;
    }

    for (Node *currentNode = leftNode; currentNode != NULL && currentNode != rightNode->next; 
         currentNode = currentNode->next)
    {
        tempBuffer[0] = currentNode->symbol;
        write(STDOUT_FILENO, tempBuffer, 1);
    }

    write(STDOUT_FILENO, "\n", 2);
}


/**
 * Функция для освобождения памяти, занятой лентой
 */
void releaseTapeMemory(LinkedTape *tape) {
    Node *currentNode = tape->head;

    while (currentNode != 0) {
        Node *nextNode = currentNode->next;
        releaseMemory(currentNode);
        currentNode = nextNode;
    }

    tape->head = tape->tail = 0;
    tape->size = 0;
}


/**
 * Функция для инициализации ленты
 */
void initializeTape(LinkedTape *tape) {
    tape->head = 0;
    tape->tail = 0;
    tape->size = 0;
}


/**
 * Функция для добавления узла в конец ленты
 */ 
void appendNode(LinkedTape *tape, char symbol) {
    Node *newNode = (Node *)allocateMemory(sizeof(Node));;
    if (newNode == 0) {
        write(STDERR_FILENO, "Memory allocation error!\n", 26);

        return;
    }

    newNode->next = 0;
    newNode->previous = 0;
    newNode->symbol = symbol;

    if (tape->head == 0 && tape->tail == 0) {
        tape->head = newNode;
        tape->tail = newNode;
    } else {
        tape->tail->next = newNode;
        newNode->previous = tape->tail;
        tape->tail = newNode;
    }

    tape->size++;
}


/**
 * Функция для добавления узла в начало ленты
 */ 
void prependNode(LinkedTape *tape, char symbol) {
    Node *newNode = (Node *)allocateMemory(sizeof(Node));
    if (newNode == 0) {
        write(STDERR_FILENO, "Memory allocation error!\n", 25);
        return;
    }

    newNode->next = 0;
    newNode->previous = 0;
    newNode->symbol = symbol;

    if (tape->head == 0 && tape->tail == 0) {
        tape->head = newNode;
        tape->tail = newNode;
    } else {
        tape->head->previous = newNode;
        newNode->next = tape->head;
        tape->head = newNode;
    }

    tape->size++;
}


/**
 * Функция чтения состояния ленты и заполнение двусвязного списка ленты
 */
void readInitialTapeState(LinkedTape *tape, char *initialTape) {
    unsigned int readBytesAmount = read(STDIN_FILENO, initialTape, BUFFER_SIZE - 1);
    if (readBytesAmount <= 0) {
        return;
    }
    if (initialTape[readBytesAmount - 1] == '\n') {
        initialTape[readBytesAmount - 1] = '\0';
    } else {
        initialTape[readBytesAmount] = '\0';
    }

    unsigned int i = 0;
    while (initialTape[i] != '\0') {
        if (initialTape[i] == ' ') {
            initialTape[i] = '_';
        }
        i++;
    }

    for (int i = 0; initialTape[i] != '\0'; i++) {
        appendNode(tape, initialTape[i]);
    }
}


/**
 * Функция чтения названия файла с консоли
 */
void readFilename(char* filename) {
    unsigned int readBytesAmount = read(STDIN_FILENO, filename, FILENAME_SIZE - 1);

    if (readBytesAmount <= 0) {
        filename[0] = '\0';

        return;
    }

    if (filename[readBytesAmount - 1] == '\n') {
        filename[readBytesAmount - 1] = '\0';
    } 
    else {
        filename[readBytesAmount] = '\0';
    }
}


/**
 * Чтение таблицы переходов и её инициализация
 */
void readTransitionTable(int fileDescriptor) {
    char buffer[BUFFER_SIZE];

    long readBytesAmount = readline(fileDescriptor, buffer, BUFFER_SIZE - 1);
    if (readBytesAmount <= 0) {
        write(STDERR_FILENO, "Reading amount of transitions fail!", 36);

        return;
    }
    if (buffer[readBytesAmount - 1] == '\n') {
        buffer[readBytesAmount - 1] = '\0';
    } else {
        buffer[readBytesAmount] = '\0';
    }

    transitionAmount = 0;
    for (int i = 0; i < readBytesAmount; i++) {
        if (buffer[i] >= '0' && buffer[i] <= '9') {
            transitionAmount = 10 * transitionAmount + buffer[i] - '0';
        } else {
            write(STDERR_FILENO, "Reading amount of transitions fail!", 36);

            return;
        }
    }

    for (int i = 0; i < transitionAmount; i++) {
        readBytesAmount = readline(fileDescriptor, buffer, BUFFER_SIZE - 1);
        if (readBytesAmount <= 0) {
            write(STDERR_FILENO, "Reading transition fail!", 25);

            return;
        }
        if (buffer[readBytesAmount - 1] == '\n') {
            buffer[readBytesAmount - 1] = '\0';
        } else {
            buffer[readBytesAmount] = '\0';
        }

        // Парсим строку (s, c) -> (s', c', d)
        Transition *transition = (Transition*)allocateMemory(sizeof(Transition));
        
        int position = 0;

        if (buffer[position] == '(') {
            position++;
        }

        transition->currentSymbol = buffer[position++];

        if (buffer[position] == ',' && buffer[position + 1] == ' ') {
            position += 2;
        }

        int statePosition = 0;
        while (buffer[position] != ')') {
            transition->currentState[statePosition++] = buffer[position++];
        }
        transition->currentState[statePosition] = '\0';

        position += 6;
        transition->newSymbol = buffer[position++];
        position += 2;

        statePosition = 0;
        while (buffer[position] != ',') {
            transition->newState[statePosition++] = buffer[position++];
        }
        transition->newState[statePosition] = '\0';

        position += 2;
        transition->direction = buffer[position];

        insertTransition(transition);
    }
}


/**
 * Функция для выполнения программы машины Тьюринга
 */
void runTuringMachine(LinkedTape *tape) {
    char currentState[BUFFER_SIZE] = "start";

    Node *currentNode = tape->head;
    while (currentNode->symbol == '_' || currentNode->symbol == ' ') {
        currentNode = currentNode->next;
    }
    
    int amountOfSteps = 0;
    while (!(currentState[0] == 's' &&
            currentState[1] == 't' &&
            currentState[2] == 'o' &&
            currentState[3] == 'p' && 
            currentState[4] == '\0')) {
        Transition *transition = findTransition(currentNode->symbol, currentState);
        if (transition == 0) {
            write(STDERR_FILENO, "No transition found!\n", 22);
            
            return;
        }

        currentNode->symbol = transition->newSymbol;

        int j = 0;
        while (transition->newState[j] != '\0') {
            currentState[j] = transition->newState[j];
            j++;
        }
        currentState[j] = '\0';

        if (transition->direction == '>') {
            if (currentNode->next == 0) {
                appendNode(tape, '_');
            }
            currentNode = currentNode->next;
        } else if (transition->direction == '<') {
            if (currentNode->previous == 0) {
                prependNode(tape, '_');
            }
            currentNode = currentNode->previous;
        }

        amountOfSteps++;
        if (amountOfSteps == MAX_STEP_AMOUNT) {
            write(STDERR_FILENO, "Amount of steps bigger than maximum step amount\n", 36);
            break;
        }
    }
}

int main() {
    char filename[FILENAME_SIZE];
    char initialTape[BUFFER_SIZE];
    LinkedTape tape;
    initializeTape(&tape);

    write(STDOUT_FILENO, "Enter file name:\n", 18);
    readFilename(filename);
    if (filename[0] == '\0') {
        write(STDERR_FILENO, "Reading filename error!\n", 25);
        printTape(&tape);
        releaseAllMemory();

        return 1;
    }

    int fileDescriptor = open(filename, O_RDONLY);
    if (fileDescriptor == -1) {
        write(STDERR_FILENO, "Opening file error!\n", 21);
        printTape(&tape);
        releaseAllMemory();

        return 1;
    }

    write(STDOUT_FILENO, "Enter initial tape state:\n", 27);
    readInitialTapeState(&tape, initialTape);
    if (tape.head == 0 && tape.tail == 0) {
        write(STDERR_FILENO, "Reading initial tape error!\n", 29);
        close(fileDescriptor);
        releaseAllMemory();

        return 1;
    }

    readStates(fileDescriptor);
    readTransitionTable(fileDescriptor);
    runTuringMachine(&tape);

    printTape(&tape);
    close(fileDescriptor);
    releaseAllMemory();

    return 0;
}