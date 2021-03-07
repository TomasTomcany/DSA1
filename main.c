#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<time.h>


// globalna premenna pre funkciu memory_init
void* start;

void memory_init(void* ptr, unsigned int size) {
    start = ptr;
    // nastavenie celeho pola na -1 namiesto garbage hodnot pre lepsi debugging
    memset(start, -1, size);

    size -= size % 4;                       // zarovnanie pamate na delitel 4

    // v hlavicke zaciatku nastavime velkost celeho volneho bloku, musime odpocitat od zadanej velkosti velkost hlavicky
    // a konca pre buducnost na rozlisenie ci je blok alokovany alebo nie rozlisime znamienkami pri velkosti bloku v
    // hlavicke, alokovany blok ma +, nealokovany -
    *((int *)ptr) = -((int)size - 3*sizeof(int));
    *((int *)(ptr+size-2*sizeof(int))) = -((int)size - 3*sizeof(int));
    // nastavenie konca
    *((int *)(ptr+size-sizeof(int))) = 1;
}

void* memory_alloc(unsigned int size) {
    if (size % 4 != 0) {
        size += (4 - (size % 4));             // padding, na rychlejsi presun medzi hlavickami a prevencia fragmentacie
    }

    // metodou first fit najdeme volne miesto pre blok
    int *current = start;                      // pointer prehladavanych hlaviciek nastavime na zaciatok
    while (*current != 1){                     // kym sa nedostane na koniec
        int block_size = abs(*current);
        // ak sa najde volny blok, do ktoreho sa nas alokovany blok zmesti, prestan prehladavat nasu haldu
        if (*current < 0 && block_size  > size+2*sizeof(int)){
            break;
        }
        // posun na dalsiu hlavicku
        current = (current + (block_size  + 2*sizeof(int))/4);
    }

    // pokial sa nenasiel ziadny volny blok do ktoreho by sa nas alokovatelny blok, nealokujemeho a funkcia vrati NULL
    if(*current == 1){
        return NULL;
    }

    // pridanie alokovaneho bloku do volneho bloku, vytvorenie noveho volneho bloku zo zvysku pomocou metody splitting
    int original_size = -(*current);

    // ak novy volny blok by mal mat menej nez 8 bytov(na hlavicku a paticku), pridaj do alokovaneho bloku
    if (original_size - size < 8){
        size += original_size;
    }

    (*current) = (int)size;                                  // hlavicka
    int *footer = (current+(size+sizeof(int))/4);            // posun na paticku
    *footer = (int)size;                                     // paticka

    // velkost noveho prazdneho bloku je original velkost - velkost alokovaneho bloku - velkost hlavicky a paticky
    int free_size = -(original_size - size - 2*sizeof(int));
    int *free_block = ((int *)footer+sizeof(int)/4);                            // pointer volneho bloku
    (*free_block) = free_size;                                                  // hlavicka
    // posun do paticky o velkost volneho bloku + velkost hlavicky
    free_block = (free_block-(free_size-sizeof(int))/4);
    (*free_block) = free_size;                                                  // paticka

    return ((void *)current+sizeof(int));                               // pointer ma ukazovat na prve slovo v bloku
}

int memory_free(void* valid_ptr) {
    if (valid_ptr == NULL){
        return 1;
    }

    valid_ptr -= sizeof(int);                          // presun na hlavicku bloku
    int size = *((int *)valid_ptr);
    // zmena znamienka na nastavenie alokovaneho bloku na volny
    *((int *)valid_ptr) =-size;

    // coalescing - spajanie volnych blokov nachadzajucich sa vedla seba na znizenie fragmentacie pamate
    // najpr sa pozrieme na nasledujuci blok a pripadne spojime
    int *next = ((int *)valid_ptr + (size + 2*sizeof(int))/4);
    if (*next <= -1){            // ak je blok nealokovany
        // zmenim velkost v hlavicke uvolneneho bloku na povodna + nasledujuca + velkost hlavicky a paticky,
        // ktorych pamat bude uvolnena na alokovanie
        int new_size = -size + *next - 2*sizeof(int);
        *((int *)valid_ptr) = new_size;
        // uvolnenie hlavicky nasledujuceho a paticky uvolneneho bloku pre debugging
        memset(((void *)next-sizeof(int)), -1, 2*sizeof(int));
        int *new_footer = ((int *)valid_ptr-(new_size-sizeof(int))/4);
        *new_footer = new_size;             //nastavenie novej paticky
    }
    else{                       // ak je blok alokovany
        int *new_footer = ((int *)valid_ptr+(size+sizeof(int))/4);
        *new_footer = -size;    // zmena znamienka paticky
    }

    // ak zadany pointer je zaciatok haldy, neexistuje predosly blok a mozme funkciu skoncit
    if (valid_ptr == start){
        return 0;
    }

    // analogicky proces spajania blokov pre predchadzajuci blok
    int *prev_foot = (int *)valid_ptr - 1;                                     // pointer paticky predosleho bloku
    if (*prev_foot <= -1) {
        int *prev = ((int *)valid_ptr + (*prev_foot - 2*sizeof(int))/4);       // pointer predoslej hlavicky
        // pripocitame velkost hlavicky dealokovaneho bloku k predchadajucemu bloku
        *prev +=  (*((int *)valid_ptr) - 2*sizeof(int));
        int prev_size = *prev;
        // uvolnime paticku predchajuce bloku a hlavicku dealokovaneho bloku pre debugging
        memset((void *)prev_foot,-1,2*sizeof(int));
        // najdeme pointer paticky dealokovaneho bloku a zmenime hodnotu velkosti
        int *footer = prev-(prev_size-sizeof(int))/4;
        *footer = prev_size;
    }
    return 0;
}

int memory_check(void* ptr) {
    if (ptr == NULL){                          // NULL pointer je neplatny
        return 0;
    }
    ptr -= sizeof(int);                        // nastavenie pointra na alokovany blok na hlavicku
    int *current = start;                      // nastavime pointer prechadzania haldy hlavicku po hlavicke na zaciatok
    int size = *((int *)current);
    while (size != 1){                         // pokial sa nedostaneme na koniec haldy
        if ((size > 1) && (current == ptr)){   // pokial je blok alokovany a pointre ukazuju na rovnaku adresu
            return 1;
        }

        current += (abs(size) + 2 * sizeof(int))/4;    // posun na dalsiu hlavicku
        size = *((int *) current);
    }
    return 0;
}

void debug_tester(){
    char region[150];
    memory_init(region, 101);
    char *ptr1 = (char *) memory_alloc(10);
    int a1 = memory_check(ptr1);
    char *ptr2 = (char *) memory_alloc(30);
    int a2 = memory_check(ptr2);
    char *ptr3 = (char *) memory_alloc(10);
    int a3 = memory_check(ptr3);
    char *ptr4 = (char *) memory_alloc(40);          // alokacia bloku ktory sa nezmesti
    int a4 = memory_check(ptr4);
    memory_free(ptr1);                                   // uvolnenie bloku na zaciatku, funkcia sa nepozera mimo pamat
    int a5 = memory_check(ptr1);
    char *ptr5 = (char *) memory_alloc(8);
    memory_free(ptr3);                                   // spajanie s nasledujucim blokom
    int a6 = memory_check(ptr3);
    memory_free(ptr2);                                   // spajanie s predchadzajucim blokom
    int a7 = memory_check(ptr2);
    int a = 40;
    int a8 = memory_check(&a);                           // memory check na adresu mimo pamate
    int a9 = memory_check(ptr2+2);                   // memory check v vnutri bloku
}

void tester(char *region, int min_memory, int max_memory, int min_block, int max_block, int random_blocks){
    int ideal = 0;                  // idealny pocet alokovanych bytov
    int ideal_blocks = 0;           // idealny pocet alokovanych blokov
    int real_blocks = 0;            // realny pocet alokovanych blokov
    char *ptr[100000];              // pole pointrov na ulozenie pre memory_alloc na uvolnenie pomocou memory_free
    int i = 0;                      // index pola pointrov


    int memory = (rand() % (max_memory-min_memory+1)) + min_memory;    // nahodna velkost pamate vyhradena v parametroch
    memory_init(region, memory);

    int block = (rand() % (max_block-min_block+1)) + min_block;
    while(memory - ideal > min_block){
        // dokym v idealnom rieseni nebude dostatok volnej pamate na alokovanie najmensieho bloku
        if (random_blocks == 1){   // ked maju byt bloky nerovnake
            block = (rand() % (max_block-min_block+1)) + min_block;
        }

        // kalkulacia idealneho pripadu
        ideal += block;
        ideal_blocks++;

        // simulacia alokacie
        ptr[i] = memory_alloc(block);
        if (ptr[i] != NULL){     // ak sa blok uspesne alokoval mozeme ho pripocitat do poctu realne alokovanych blokov
            real_blocks++;
            i++;

        }
    }

    // uvolnenie pamati
    for (int j = 0; i <= j; j++){
        memory_free(ptr[i]);
    }


    // vyhodnotenie % uspesnosti alokovania oproti realnemu rieseniu
    float block_result = ((float)real_blocks/(float)ideal_blocks)*100;
    printf("Na pamati o velkosti %d bytov bolo alokovanych "
           "%.2f%% blokov v porovnani s idealnym riesenim.\n", memory, block_result);
}


int main() {
    // po spusteni by nemal davat ziadny error, sledovanie pamate v debugging tool ci vsetko funguje tak ako ma
    debug_tester();

    char region[1000000];
    srand(time(0));
    for (int i = 0; i < 10;i++) {
        tester(region, 50, 200, 8, 24, 0);                        // 1. scenar
        //tester(region, 50, 200, 8, 24, 1);                        // 2. scenar
        //tester(region, 5000, 500000, 500, 5000, 1);               // 3. scenar
        //tester(region, 50000, 1000000, 8, 50000, 1);              // 4. scenar
    }
    return 0;
}