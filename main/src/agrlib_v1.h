#ifndef AGREGATOR_LIB
#define AGREGATOR_LIB

#include <stdio.h>
#include <stdlib.h>
#define TEST_CASE

//constants
char sep = ',';
int end_CR = 0x0D;
int end_LF = 0x0A; // end of line symbol
size_t timestamp_col = 9; // 0,1,2,3,4, ... ,timestamp_col, .... n - holds the number of timestamp column
int timestamp_len = 10; // length of timestamp, with all symbols  included
/* Programm acepts all constant length timestaps following the ISO 8601 (https://en.wikipedia.org/wiki/ISO_8601)*/
int max_try = 5;//attampts to allocate memory


FILE* input_csv;
char *csv = NULL; // csv in memory
char *csv_end = NULL; // end of csv in memory
static size_t csv_size = 0;
static size_t rows = 0;
static size_t col  = 0;


static char ** timestamps = NULL; // each timestamp start
static char **lines = NULL; // each line start
size_t max_line_len = 0; // maximum line length, EOL counted "abc\n" = 4

//functions
int csv_import(char path_to_csv[] );
int csv_info( char * csv); // fills csv info = rows + col
int init_memory(size_t len, int case_ ); // init memory for timestamps and lines
int csv_get_lines(char * csv); // fills lines
int csv_get_timestamps(char *csv); // fills timestamps
int sort_timepointers( char **time, char** line);
int compare_timestamps(char *first, char *second); // comparing by f-s ? <>=0
size_t new_gap(size_t gap); // generates new gap for combo sorting
char *** agregate(char path_to_csv[]); //main function

 #ifdef TEST_CASE
//help functions
void show_hiden_time(char * timestamp);
void show_hiden_string(char *line);
void general_check(int case_);
#endif



int csv_import(char path_to_csv[] ){
if(path_to_csv){
    input_csv = fopen(path_to_csv, "rb");
    if(input_csv != NULL){
        fseek(input_csv, 0L, SEEK_END);
        csv_size = ftell(input_csv);
        if(csv_size){
            csv = calloc(csv_size, sizeof(char));
            char *local_csv = csv; // save reference to csv
            if(csv != NULL){
                csv_end = csv + csv_size;
                fseek(input_csv, 0L, SEEK_SET);
                size_t read = fread(csv, sizeof(char),csv_size, input_csv);
                if (read != csv_size){
                    #ifdef TEST_CASE
                    printf("%li - read, %li - size", read,csv_size);
                    #endif // TEST_CASE
                    free(local_csv);
                    fclose(input_csv);
                    #ifdef TEST_CASE
                    printf("import error\n ");
                    #endif // TEST_CASE
                }else{
                    #ifdef TEST_CASE
                    printf("import OK\n");
                    #endif // TEST_CASE
                    return 1;
                }
            }else{
                #ifdef TEST_CASE
                printf("mem error\n");
                #endif // TEST_CASE
                return 0;

            }
        }else{
            #ifdef TEST_CASE
            printf("empty csv\n");
            #endif // TEST_CASE
            return 0;

        }
    }else{
        #ifdef TEST_CASE
        printf("could not open csv\n");
        #endif // TEST_CASE
        return 0;

    }
}else{
    #ifdef TEST_CASE
    printf("wrong path");
    #endif // TEST_CASE
    return 0;
}


    return 0;
}

int csv_info( char * csv){ // will also change EOL symbol with '\0'
    char current_char;
    int status = 0;
    char *org = csv;
    for(size_t i = 0; i < csv_size; i++){
        current_char = *(csv++);
        printf("%c", current_char);
        if(current_char == sep){
            col ++;
        }else if((current_char == end_CR) || (current_char == end_LF) ){
            col++;  // ... , ... , ... has 3 col, but 2 sep
            status = 1; // more than 1 line and len is counted
            break;
        }
    }

    if(!status){return 0;} // only 1 line, = header
    csv = org;
    size_t max_len = 0;
    size_t cur_len = 0;
    int line_is_ready = 0;

    for(size_t i = 0; i < csv_size; i++){
    current_char = *csv;
        if(current_char == end_CR || current_char == end_LF){
            if( ! line_is_ready){
            //looking for max_line_len
                if(cur_len > max_len){
                    max_len = cur_len;  // max len is the number of char without EOL
                }
                // this line is not more intrsting
                rows++;
                cur_len = 0;
                *csv = '\0'; // this way  '\0' is first unprintable char
                line_is_ready = 1;
            }else{
                csv ++;
                continue;
            }
        }else {
            if(line_is_ready){line_is_ready = 0;}else{cur_len ++;}
        }
        csv ++;
    }
    max_line_len = ++max_len;
    #ifdef TEST_CASE
    printf("%li - col, %li - lines, %li - max len\n", col,rows,max_line_len );
    #endif
    return 1;
}

int init_memory(size_t len, int case_ ){
    switch (case_){
        case 1:
            timestamps = calloc(len, sizeof( char **));
            if(timestamps == NULL){
                #ifdef TEST_CASE
                printf("error allocating memory for timestamps\n");
                #endif
                return 0;
            }
            return 1;
        case 2:
            lines = calloc(len, sizeof( char **));
            if(lines == NULL){
                #ifdef TEST_CASE
                printf("error allocating memory for lines\n");
                #endif
                return 0;
                }
            return 1;
        case 3:
            lines = calloc(len, sizeof( char **));
            timestamps = calloc(len, sizeof( char **));
            if(lines  == NULL & timestamps == NULL){
                #ifdef TEST_CASE
                printf("error allocating ANY memory\n");
                #endif
                return 0;
            }
            return 1;
        }
    return 0;
    }

int csv_get_lines(char *csv){

    if(lines == NULL){
        if(! init_memory(rows, 2)){
            int success = 0;
                for(int i = 0; i < max_try; i++){
                    if(init_memory(rows, 2) ){ success = 1;break;}
                }
                if(! success){
                    #ifdef TEST_CASE
                    printf("FINAL : can not allocate memory for lines\n");
                    #endif
                    return 0;
                }
            }
        }

    char ** tmp = lines;
    int first_char = 1;
    char current_char;
    for(size_t i = 0; i < csv_size; i++){
        current_char = *csv;
        if(current_char == end_CR || current_char == end_LF || current_char == '\0'){
            first_char = 1;
        }else{
            if(first_char){
                *lines = csv;
                lines++;
                first_char = 0;
            }
        }
        csv ++;
    }
    lines = tmp;
    #ifdef TEST_CASE
    printf("--- first letters of csv---\n");
    for(size_t i = 0; i<rows; i++){
        printf(" %c ", **(tmp++));
    }
    printf("\n");
    #endif
    return 1;
}

int csv_get_timestamps(char *csv){
    if(timestamps == NULL){
            if(! init_memory(rows, 1)){
                int success = 0;
                    for(int i = 0; i < max_try; i++){
                        if(init_memory(rows, 1) ){ success = 1;break;}
                    }
                    if(! success){
                        #ifdef TEST_CASE
                        printf("FINAL : can not allocate memory for timestamps\n");
                        #endif
                        return 0;
                    }
                }
            }
        char ** tmp = timestamps;
        char current_char;
        int found = 0;
        size_t counter = 0; // count columns
        for(size_t i = 0; i < csv_size; i++){
            current_char = *csv;
            if(current_char == end_CR || current_char == end_LF || current_char == '\0'){
                if(counter != col){
                    return 0; //there is an error in csv file : not equal number of columns at each line
                }
                found =0;
                counter = 0;
            } else if( (counter == timestamp_col) & (found == 0) ){
                if(current_char != sep){
                    *timestamps = csv;
                    #ifdef TEST_CASE
                    show_hiden_time(*timestamps);
                    #endif
                    timestamps++;
                    found = 1;
                }
            }
            csv++;
            if(current_char == sep){
                counter ++;
                if(counter > col){
                    return 0; //there is an error in csv file : not equal number of columns at each line
                }
            }
        }
        timestamps = tmp;
        return 1;
}

#ifdef TEST_CASE
void show_hiden_time(char * timestamp)
{
printf ("%s", "+ ");
for (int i = 0; i < timestamp_len ; i++)
{
    printf ("%c", *(timestamp++));
}
    printf ("\n");
}
#endif

#ifdef TEST_CASE
void show_hiden_string (char *line)
{
    char current_char;
    printf ("%s", "+ str : ");
    for (int i = 0; i < max_line_len ; i++)
    {
         current_char = *(line++);
         if(current_char == end_CR || current_char == end_LF || current_char == '\0'){break;}
         printf ("%c", current_char);
    }
}
#endif

int compare_timestamps(char *first, char *second)
/*the comparation is done by subtraction and  comparation with zero, so it is important to follow timestamp format*/
{
  // return value
  // 1 if  first  >  second
  // 2 if  first  <  second
  // 3 if  first  =  second
#ifdef TEST_CASE
  char *first_ = first;
  char *second_ = second;
  printf ("---to compare: \n");
  show_hiden_time (first_);
  show_hiden_time (second_);
  printf ("--- \n");
#endif

  for (int i = 0; i < timestamp_len; i++)
    {
      if (*first - *second > 0)
	{
#ifdef TEST_CASE
	  printf ("   --- F>S(1) \n");
#endif
	  return 1;
	}
      else if (*first - *second < 0)
	{
#ifdef TEST_CASE
	  printf ("   --- F<S(2) \n");
#endif
	  return 2;
	}
      first++;
      second++;
    }
#ifdef TEST_CASE
  printf ("   --- F=S(3) \n");
#endif
  return 3;

}

size_t new_gap(size_t gap)
{
  gap = (gap * 10) / 13;
  if (gap == 9 || gap == 10)
    gap = 11;
  if (gap < 1)
    gap = 1;
  return gap;
}

int sort_timepointers( char **time, char** line){
/* sorting with combo sort algorithm */
#ifdef TEST_CASE
  printf (">>> COMPARE unit: \n");
#endif
    if(rows<3){return 1;} //0,1,2 they are already sorted
    time ++;
    line ++; // due to the first line of csv

  size_t gap = rows - 2;
  char *old_ = NULL;
  char *new_ = NULL;
  char **working_ts = NULL;

  char *old_line = NULL;
  char *new_line = NULL;
  char **working_line = NULL;

  size_t i;
  int swapped = 0;
  for (;;)
    {
      swapped = 0;
      gap = new_gap (gap);
      working_ts = time;
      working_line = line;

      for (i = 0; i < rows - gap -1; i++)
	{
	  size_t j = i + gap;
	  //if (csv_end - (*(working_ts + i) + timestamp_len) < 0 || csv_end - (*(working_ts + j) + timestamp_len) < 0){return 0;} // error in timestamp length - we will run out of memory area(crash)

	  if (compare_timestamps (*(working_ts + i), *(working_ts + j)) ==1)
	    {
            new_ = *(working_ts + i);
            new_line = *(working_line + i);

            old_ = *(working_ts + j);
            old_line = *(working_line + j);
        #ifdef TEST_CASE
            printf ("> old values and current parametrs : \n");
            show_hiden_time(*(working_ts + i));
            show_hiden_time(*(working_ts + j));
        #endif

            *(working_ts + i) = old_;
            *(working_line + i) = old_line;
            *(working_ts + j) = new_;
            *(working_line + j) = new_line;

        #ifdef TEST_CASE
            printf ("> new values and current parametrs : \n");
            show_hiden_time(*(working_ts + i));
            show_hiden_time(*(working_ts + j));
        #endif
	      swapped = 1;
	    }
	}
        if (gap == 1 && !swapped)
        {
            return 1;
        }
    }
    return 0;
}

#ifdef TEST_CASE
void general_check(int case_){ //prints whole csv
printf("GENERAL CHECK\n");
char **tmp;
switch(case_){
    case 1:
        tmp = timestamps;
        for(size_t i = 0; i < rows; i++){
            printf("%li line ",i);
            show_hiden_time(*(tmp++));

        }
        break;

    case 2:
        tmp = lines;
        for(size_t i = 0; i < rows; i++){
            show_hiden_string(*(tmp++));
        }
        break;
}
}
#endif

char *** agregate(char path_to_csv[]){

/*csv_import ---> csv info --> init_memory ---> csv_get_lines ---> csv_get_timestamps ---> sort(compare)*/
    if(csv_import(path_to_csv)){
                if(csv_info(csv)){
                    if( csv_get_lines(csv) && csv_get_timestamps(csv)){

                        if(sort_timepointers(timestamps, lines)){
                            return &lines;
                        }
                    }
                }
            }
        return NULL;
}




#endif
