#include<stdio.h>
#include<stdlib.h>
#include<string.h>


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
    *((int *)ptr) = -(size - 3*sizeof(int));
    *((int *)(ptr+size-2*sizeof(int))) = -(size - 3*sizeof(int));
    // nastavenie konca
    *((int *)(ptr+size-sizeof(int))) = 1;
}

void* memory_alloc(unsigned int size) {
    size += (4 - size % 4);             // padding, na rychlejsi presun medzi hlavickami

    // metodou first fit najdeme volne miesto pre blok
    void *current = start;
    while (*((int *)current) != 1){                     // kym sa nedostane na koniec
        int block_size = abs(*((int *)current));
        // ak sa najde volny blok, do ktoreho sa nas alokovany blok zmesti, prestan prehladavat nasu haldu
        if (*((int *)current) < 0 && block_size  > size+2*sizeof(int)){
            break;
        }
        // posun na dalsiu hlavicku
        current = ((int *)current + (block_size  + 2*sizeof(int))/4);
    }

    // pokial sa nenasiel ziadny volny blok do ktoreho by sa nas alokovatelny blok, nealokujemeho a funkcia vrati NULL
    if(*((int *)current) == 1){
        return NULL;
    }

    // pridanie alokovaneho bloku do volneho bloku, vytvorenie noveho volneho bloku zo zvysku pomocou metody splitting
    int original_size = -(*((int *)current));

    // ak novy volny blok by mal mat menej nez 8 bytov(na hlavicku a paticku), pridaj do alokovaneho bloku
    if (original_size - size < 8){
        size += original_size;
    }

    (*((int *)current)) = size;                                 // hlavicka
    int *footer = ((int *)current+(size+sizeof(int))/4);       // posun na paticku
    *((int *)footer) = size;                                    // paticka

    // velkost noveho prazdneho bloku je original velkost - velkost alokovaneho bloku - velkost hlavicky a paticky noveho bloku
    int free_size = -(original_size - size - 2*sizeof(int));
    int *free_block = ((int *)footer+sizeof(int)/4);                            // pointer volneho bloku
    (*((int *)free_block)) = free_size;                                         // hlavicka
    // posun do paticky o velkost volneho bloku + velkost hlavicky
    free_block = ((int *)free_block-(free_size-sizeof(int))/4);
    (*((int *)free_block)) = free_size;                                         // paticka

    return current+sizeof(int);
}

int memory_free(void* valid_ptr) {
    valid_ptr -= sizeof(int);                                           // presun na hlavicku bloku
    int size = *((int *)valid_ptr);
    // zmena znamienka na nastavenie alokovaneho bloku na volny
    *((int *)valid_ptr) =-size;

    // coalescing - spajanie volnych blokov nachadzajucich sa vedla seba na znizenie fragmentacie pamate
    // najpr sa pozrieme na nasledujuci blok a pripadne spojime
    void *next = (valid_ptr + size + 2*sizeof(int));
    if (*((int *)next) <= -1){           // ak je blok nealokovany
        // zmenim velkost v hlavicke uvolneneho bloku na povodna + nasledujuca + velkost hlavicky a paticky,
        // ktorych pamat bude uvolnena na alokovanie
        int new_size = -size + *((int *)next) - 2*sizeof(int);
        *((int *)valid_ptr) = new_size;
        memset((next-sizeof(int)), -1, 2*sizeof(int));   // uvolnenie hlavicky nasledujuceho a paticky uvolneneho bloku
        void *new_footer = (void *)valid_ptr-new_size+sizeof(int);
        *((int *)new_footer) = new_size;                      //nastavenie novej paticky
    }
    else{                                 // ak je blok alokovany
        void *new_footer = (void *)valid_ptr+size+sizeof(int);
        *((int *)new_footer) = -size;
    }

    if (valid_ptr == start){             // ak zadany pointer je zaciatok haldy, neexistuje predosly blok
        return 0;
    }

    // nasledne analogicky vykoname proces pre predchadzajuci blok
    void *prev_foot = valid_ptr - sizeof(int);                           // pointer paticky predosleho bloku
    int prev_foot_size = *((int *)prev_foot);
    if (prev_foot_size <= -1) {
        void *prev = (valid_ptr + prev_foot_size - 2*sizeof(int));       // pointer predoslej hlavicky
        // pripocitame velkost hlavicky dealokovaneho bloku k predchadajucemu bloku
        *((int *)prev) +=  (*((int *)valid_ptr) - 2*sizeof(int));
        int prev_size = *((int *)prev);
        // uvolnime paticku predchajuce bloku a hlavicku dealokovaneho bloku
        memset(prev_foot,-1,2*sizeof(int));
        // najdeme pointer paticky dealokovaneho bloku a zmenime hodnotu velkosti
        void *footer = prev-prev_size+sizeof(int);
        *((int *)footer) = prev_size;
    }
    return 0;
}

int memory_check(void* ptr) {
    if (ptr == NULL){                          // NULL pointer je neplatny
        return 0;
    }
    ptr -= sizeof(int);                        // nastavenie pointra na alokovany blok na hlavicku
    int *current = start;                     // nastavime pointer prechadzania haldy hlavicku po hlavicke na zaciatok
    int size = *((int *)current);
    while (size != 1){                         // pokial sa nedostaneme na koniec haldy
        if ((size > 1) && (current == ptr)){   // pokial je blok alokovany a pointre ukazuju na rovnaku adresu
            return 1;
        }

        if (size > 1) {                               // ak je blok alokovany
            current += (size + 2 * sizeof(int))/4;      // posun na dalsiu hlavicku
        }
        else{                                         // ak je blok nealokovany
            current -= (size - 2 * sizeof(int))/4;      // posun na dalsiu hlavicku
        }

        size = *((int *) current);
    }
    return 0;
}

int main(void) {
    char heap[101];
    memory_init(&heap, 101);
    char *ptr1 = (char *) memory_alloc(10);
    int a1 = memory_check(ptr1);
    char *ptr2 = (char *) memory_alloc(30);
    int a2 = memory_check(ptr2);
    char *ptr3 = (char *) memory_alloc(10);
    int a3 = memory_check(ptr3);
    char *ptr4 = (char *) memory_alloc(40);
    int a4 = memory_check(ptr4);
    memory_free(ptr1);
    int a5 = memory_check(ptr1);
    char *ptr5 = (char *) memory_alloc(8);
    memory_free(ptr3);
    int a6 = memory_check(ptr3);
    memory_free(ptr2);
    int a7 = memory_check(ptr2);
    int a = 40;
    int a8 = memory_check(&a);
    return 0;
}