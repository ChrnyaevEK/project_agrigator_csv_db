#ifndef AGREGATOR_LIB
#define AGREGATOR_LIB

#include <stdio.h>
#include <stdlib.h>
//! For testing define TEST_CASE
#define TEST_CASE
//! DONE is used as success indicator
#define DONE 1
//! ERROR is used as fail indicator
#define ERROR 0


/*!
 *  General : first meaningfull line is header for table
 *  Empty lines are allowed
 */

//! Defines csv separator, must be single symbol
char sep = ',';
//! To handle different OS dependent EOL, EOL symbols defined
//! End of line CR symbol
int end_CR = 0x0D;
//! End of line LF symbol
int end_LF = 0x0A; 
//! This parameter is user defined. It holds the timestamps column number. Note, that columns are counted from 0 : 0,1,2,3,4, ... ,timestamp_col, .... n
size_t timestamp_col = 0;
//! Program accepts all ,constant length, timestamps which are following the ISO 8601 (https://en.wikipedia.org/wiki/ISO_8601). 
//! Length is counted automatically (all symb. included)
int timestamp_len; 
//! Max number of attempts to allocate memory
int max_try = 5;

//! File pointer
FILE* input_csv;
//! The representation of file in memory
static char *csv = NULL; 
//! Holds file size
static size_t csv_size = 0;
//! Holds rows/lines number., Header included(aut.counted)
static size_t rows = 0;
//! Holds column number(aut.counted)
static size_t col  = 0;
//! In sorting purposes we will store each timestamps position
static char **timestamps = NULL; 
// In sorting purposes we will store each line first character(line beginning) position
static char **lines = NULL; 
//! In safety purposes and memory saving, we store maximum line length, EOL counted "abc\n" = 4.(aut.counted)
static size_t max_line_len = 0; 

//! Function perform import of file.Input value: path to file. Return value: DONE or ERROR
int csv_import(char path_to_csv[] );
//! Function counts rows and columns, max length of line,also replace all EOL with NULL. Input : pointer to csv in memory. Return value: DONE or ERROR
int csv_info( char * csv); 
//! Function contains main memory initialization. Input: size to initialize pointer with(main case - number of rows) and cases 1-2-3, which pointer to initialize with memory.
//! Return value: DONE or ERROR
int init_memory(size_t len, int case_ );
//! Function loops through file searching for line beginnings positions. Input : pointer to csv in memory. Return value: DONE or ERROR
int csv_get_lines(char * csv);
//! Function loops through file searching for timestamps positions. Input : pointer to csv in memory. Return value: DONE or ERROR
int csv_get_timestamps(char *csv);
//! Function sorts timestamps. Input : array of timestamps starts pointers and array of line beginnings. Algorithm: combo sorting. Return value: DONE or ERROR 
int sort_timepointers( char **time, char** line);
//! Function compares first given timespamp with second by subtraction. Return value: 1 for F>S., 2 for F<S., 3 for F=S or  ERROR 
int compare_timestamps(char *first, char *second);
//! Function generates new gap for combo sort algorithm. Input : old gap. Return : new gap
size_t new_gap(size_t gap); 
//! Main function, controls order of program flow and 
char *** agregate(char *path_to_csv, unsigned short int *timestamp_column);
//! Help functions - in test cases may come in handy. ! STDOUT is used
#ifdef TEST_CASE
//! Function prints timestamp, given by pointer, of length timestamp_len 
void show_hiden_time(char * timestamp);
//! Function prints line given by pointer until EOL met
void show_hiden_string(char *line);
//! Function perform general check by printing: case{1) - all sorted timestamps, case(2) - all sorted csv lines 
void general_check(int case_);
#endif



int csv_import(char *path_to_csv ){
    //! Import starts with reading file. An ERROR returned in case of fail. 
    if(path_to_csv){
        input_csv = fopen(path_to_csv, "rb");
        if(input_csv != NULL){
            //! The first thing after file was opened - get its size. Then we allocate memory. All at once.
            fseek(input_csv, 0L, SEEK_END); 
            csv_size = ftell(input_csv);
            if(csv_size){
                csv = calloc(csv_size, sizeof(char));
                //!To free memory lets save the original reference
                char *local_csv = csv;
                if(csv != NULL){
                    //!If we have memory - read whole file to it and then close file
                    fseek(input_csv, 0L, SEEK_SET);
                    size_t read = fread(csv, sizeof(char),csv_size, input_csv);
                    /*!If at some stage an error occurs - ERROR is returned end the program generaly ends. Possible errors: 
                    read less than file size(import error), not enough memory to hold file(mem error), error in opening file. These are 
                    end cases and ERROR is returned. However we will consider an empty file as already sorted and return DONE 
                    */
                    if (read != csv_size){ // Error while reading ---> end
                        #ifdef TEST_CASE
                        printf("%li - read, %li - size", read,csv_size);
                        #endif // TEST_CASE
                        free(local_csv);
                        fclose(input_csv);
                        #ifdef TEST_CASE
                        printf("reading error\n ");
                        #endif // TEST_CASE
                    }else{ // Imported successfully 
                        #ifdef TEST_CASE
                        printf("import OK\n");
                        #endif // TEST_CASE
                        return DONE;
                    }
                }else{ // Not enough memory ---> end
                    #ifdef TEST_CASE
                    printf("mem error\n");
                    #endif // TEST_CASE
                    return ERROR;

                }
            }else{ //Empty ---> DONE
                #ifdef TEST_CASE
                printf("empty csv\n");
                #endif // TEST_CASE
                return DONE;

            }
        }else{ // Error in opening ---> end
            #ifdef TEST_CASE
            printf("could not open csv\n");
            #endif // TEST_CASE
            return ERROR;

        }
    }else{ // No path supplied ---> end
        #ifdef TEST_CASE
        printf("wrong path");
        #endif // TEST_CASE
        return ERROR;
    }
    return ERROR;
}

int csv_info( char * csv){ 
    char current_char;
    int status = 0;
    char *org = csv;
    //! First,lets count columns. For this we use first line, as it is considered to be header
    for(size_t i = 0; i < csv_size; i++){ 
        //! So, each character is compared with separator or EOL
        current_char = *(csv++);
        if(current_char == sep){
            //! Each found separator increase column number
            col ++;
        }else if((current_char == end_CR) || (current_char == end_LF) || current_char == '\0' ){
            //! As the EOL reached - we increase column number again : ... , ... , ... has 3 col, but only 2 separators 
            col++;  
            //! Here we will also check if file contains more than one line. 
            status = 1; 
            break;
        }
    }
    //! If only one line found - we return, as it has no reason to continue sorting
    if(!status){return DONE;} // only 1 line, = header
    csv = org;
    //! Next - count meaningful lines and maximum line length. From here line becomes sequence of characters delimited  by '\0' 
    //! (max_len - maximum length, cur_len - current length)
    size_t max_len = 0;
    size_t cur_len = 0;
    int seq_counted = 0;

    for(size_t i = 0; i < csv_size; i++){ 
        current_char = *csv;
        if(current_char == end_CR || current_char == end_LF || current_char == '\0' ){
            //! Replacing all EOL with '\0' will make searching easier 
            *csv = '\0';
            cur_len = 0;
            seq_counted = 0;
        }else{
            cur_len ++;
            if(!seq_counted){
                rows ++;
                seq_counted = 1;
            }
            if (cur_len > max_len){
                max_len = cur_len;
            }
        }
        csv++;
    }
    max_line_len = ++max_len;
    //! In test case, information about file will be printed
    #ifdef TEST_CASE
    printf("File has : %li - columns, %li - lines and max length of line is %li \n Timestamps column - %li\n", col,rows,max_line_len ,timestamp_col);
    #endif
    return DONE;
}

int init_memory(size_t len, int case_ ){
    //! For better structure some memory initialization is located here. 
    //! Global pointers timestamp and lines getting memory from here. Input: len - size of memory to allocate, case_ - for which pointer(1,2,3)
    switch (case_){
        case 1:
            timestamps = calloc(len, sizeof( char **));
            if(timestamps == NULL){
                #ifdef TEST_CASE
                printf("error allocating memory for timestamps\n");
                #endif
                return ERROR;
            }
            return DONE;
        case 2:
            lines = calloc(len, sizeof( char **));
            if(lines == NULL){
                #ifdef TEST_CASE
                printf("error allocating memory for lines\n");
                #endif
                return ERROR;
                }
            return DONE;
        case 3:
            lines = calloc(len, sizeof( char **));
            timestamps = calloc(len, sizeof( char **));
            if(lines  == NULL & timestamps == NULL){
                #ifdef TEST_CASE
                printf("error allocating ANY memory\n");
                #endif
                return ERROR;
            }
            return DONE;
        }
    return ERROR;
}

int csv_get_lines(char *csv){
    //! Getting lines beginnings starts with memory allocation. We need memory for pointers array with length equal to the number of rows 
    if(lines == NULL){
        //! In case of fail we will try up try max try number
        if(! init_memory(rows, 2)){
            int success = 0;
                for(int i = 0; i < max_try; i++){
                    if(init_memory(rows, 2) ){ success = 1;break;}
                }
                if(! success){
                    #ifdef TEST_CASE
                    printf("FINAL : can not allocate memory for lines\n");
                    #endif
                    return ERROR;
                }
            }
        }


    //! Then we search for position of each first not null symbol and save reference on it
    char ** tmp = lines;
    int first_char = 1;
    char current_char;
    for(size_t i = 0; i < csv_size; i++){
        current_char = *csv;
        if(current_char == '\0'){
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
    general_check(2);
    #endif
    return DONE;
}

int csv_get_timestamps(char *csv){
    //! Getting timestamps pointers for further comparison start with memory a allocation. We try to allocate memory as well several times
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
                        return ERROR;
                    }
                }
            }
    //! To indicate if current position in file contains timestamp or not we will count columns
    //! If file contains an error and we exceed the number of columns -work  stops.
    size_t counter_col = 0; // Count columns
    //! As timestamp is found and its first character address  is stored - we do not expect timestamps in this line any more
    char ** tmp = timestamps;
    char current_char;
    int found = 0;
    //! All timestamps should be the same length, which is never exceeded during timestamps comparison 
    int counter_ts = 0;
    int length_of_ts_counted = 0;
    int is_first_char_of_ts = 1;
    //! Header does not take part in counting, so first - we must pass it
    int header_passed = 0;
    //! To avoid errors in column number we indicate step from meaningful part to meaning less
    //! If at this moments we did not met enough separators - error
    int inline_indicator = 0;
    for(size_t i = 0; i < csv_size; i++){
        //! Each symbol is checked for separator or EOL
        current_char = *csv;
        if(current_char == '\0' ||  current_char == EOF){ 
            //! EOL means that we are in useless part
            if(counter_col == timestamp_col && !length_of_ts_counted && header_passed && counter_ts ){
                //! If timestamp is located at the end of line - its length is counted here.
                timestamp_len = counter_ts;
                length_of_ts_counted = 1;
            }
            if((counter_col || inline_indicator) && (++counter_col != col)){
                //! As well at the end of line we check for number of columns to be the same as counted in header
                #ifdef TEST_CASE
                printf(" Error in csv file - wrong columns sum\n");
                #endif
                return ERROR;
            }
            if(header_passed && counter_col && is_first_char_of_ts){
                //! Another error is reaching end of line and without reaching timestamp column. 
                #ifdef TEST_CASE
                printf(" Error in timestamp col number\n");
                #endif
                return ERROR;
            }
            //! Indication of non header line is : timestamp length counting has started, but header_passed is still false .
            if(counter_ts && !header_passed){header_passed = 1;}
            counter_col = 0;
            is_first_char_of_ts = 1;
            if(counter_ts && timestamp_len && counter_ts != timestamp_len ){ 
                //! Next error is different length of timestamps, which means that  error if no timestamp found. If something else is in column of ts,
                //! but of the same length - it ok, as well it will be sorted.
                #ifdef TEST_CASE
                printf("error in timestamp len\n");
                #endif
                return ERROR;
            }

            inline_indicator = 0;
            counter_ts = 0;            
        }else{
            if(!inline_indicator){inline_indicator = 1;}
            if(current_char == sep){
                if(counter_col == timestamp_col && !length_of_ts_counted && header_passed && counter_ts){
                    //! Length of timestamp is as well controlled/counted when separator is met.
                    timestamp_len = counter_ts;
                    length_of_ts_counted = 1;
                }
                counter_col ++;
                if(counter_col > col){
                    #ifdef TEST_CASE
                    printf(" Error in csv file - wrong columns sum\n");
                    #endif
                    return ERROR;
                }
                if(counter_ts && timestamp_len && counter_ts != timestamp_len ){ 
                    return ERROR;
                }
            }
            if(counter_col == timestamp_col && current_char != sep){
                //! Finally - an address  of first symbol in timestamps columns is taken.
                if(is_first_char_of_ts){
                    is_first_char_of_ts = 0;
                    *timestamps = csv;
                    //show_hiden_time(csv);
                    timestamps++;
                }
                counter_ts ++;
                
            }
        }
        csv++;
        if((csv_size - i == 1) && inline_indicator ){
            current_char = '\0';
            i--;
        }
    }
    timestamps = tmp;
    #ifdef TEST_CASE
    general_check(1);
    #endif
    return DONE;
}

#ifdef TEST_CASE
void show_hiden_time(char * timestamp)
{
    //! Helpful when testing. Show timestamps by pointer
    char current_char;
    printf ("%s", "+ ");
    for (int i = 0; i < timestamp_len ; i++)
    {
        current_char = *(timestamp++);
        if(current_char == '\0'){ break;    }
        printf ("%c", current_char);
    }
        printf ("\n");
}
#endif

#ifdef TEST_CASE
void show_hiden_string (char *line)
{
    //! Helpful when testing. Show line by its pointer
    char current_char;
    printf ("%s", "+ str : ");
    for (int i = 0; i < max_line_len ; i++)
    {
         current_char = *(line++);
         if(current_char == '\0'){break;}
         printf ("%c", current_char);
    }
    printf("\n");
}
#endif


int compare_timestamps(char *first, char *second)
{
    //! Comparison is done by subtraction and  comparison with zero, so it is important to follow timestamps format
    //! Return value
    //! 1 if  first(passed value)  >  second (passed value) 
    //! 2 if  first  <  second
    //! 3 if  first  =  second
    //! In TEST_CASE some additional information is shown 
    #ifdef TEST_CASE
      char *first_ = first;
      char *second_ = second;
      printf ("---to compare: \n");
      show_hiden_time (first_);
      show_hiden_time (second_);
    #endif
  //! Comparing we loop through timestamps subtracting values
  //! Return values reflects relations between First and Second values
  for (int i = 0; i < timestamp_len; i++){
      if (*first - *second > 0){
        #ifdef TEST_CASE
        printf ("   F > S(1) \n");
        #endif
        return 1;
    }
      else if (*first - *second < 0){
        #ifdef TEST_CASE
        printf ("   F < S(2) \n");
        #endif
        return 2;
    }
      first++;
      second++;
    }
    #ifdef TEST_CASE
    printf ("   F=S(3) \n");
    #endif
    return 3;
}

size_t new_gap(size_t gap)
{
    //! New gap for sorting is formed from rows number and after first loop of algorithm from last gap. Conditions of gap formation could be changed 
    gap = (gap * 10) / 13;
    if (gap == 9 || gap == 10)
    gap = 11;
    if (gap < 1)
    gap = 1;
    return gap;
}

int sort_timepointers( char **time, char** line)
{
    //! Algorithm is based on bubble sort. The only difference is in shrinking gap between two compared values. At last stages it becomes pure bubble sort.
    #ifdef TEST_CASE
    printf (">>> COMPARE unit: \n");
    #endif
    //! Before comparison we would check if it worth to be done - if csv has only 2 rows it has no meaning.
    if(rows<3){return 1;} //0,1,2 they are already sorted
    //! In other case, first thing to do - step over header.
    time ++;
    line ++; 
    //! Then define some temporary holders for compared objects
    //! We compare timestamps but the line order gets changed too
    size_t gap = rows - 2;
    char *new_ = NULL;
    char **working_ts = NULL;
    char *new_line = NULL;
    char **working_line = NULL;

    size_t i;
    int swapped = 0;
  for (;;)
    {
        //! Sorting loop starts with getting new gap
        swapped = 0;
        gap = new_gap (gap);
        working_ts = time;
        working_line = line;
        for (i = 0; i < rows - gap -1; i++){
            //! Then we compare two timestamps with compare_timestamps function. IF YOU NEED TO CHANGE ORDER - SET 2 INSTEAD OF 1 IN IF CONDITION 
            //! If the order is ok, go on, else - swap the pointers 
            size_t j = i + gap;
            if (compare_timestamps (*(working_ts + i), *(working_ts + j)) == 1)
            {
                new_ = *(working_ts + i);
                new_line = *(working_line + i);
            #ifdef TEST_CASE
                printf ("> old values: \n");
                show_hiden_time(*(working_ts + i));
                show_hiden_time(*(working_ts + j));
            #endif

                *(working_ts + i) = *(working_ts + j);
                *(working_line + i) = *(working_line + j);
                *(working_ts + j) = new_;
                *(working_line + j) = new_line;

            #ifdef TEST_CASE
                printf ("> new values: \n");
                show_hiden_time(*(working_ts + i));
                show_hiden_time(*(working_ts + j));
            #endif
              swapped = 1;
            }
    }
        if (gap == 1 && !swapped)
        {
            return DONE;
        }
    }
    return ERROR;
}

#ifdef TEST_CASE
void general_check(int case_)
{
    //! Shows all timestamps(1), saved by pointers or lines(2), saved by pointers
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

char *** agregate(char *path_to_csv, unsigned short int *timestamp_column)
{
//! This is main logic function. Call this with path to perform all. 
//! Program flow : csv_import ---> csv_info --> init_memory ---> csv_get_lines ---> csv_get_timestamps ---> sort(compare) END
//! file import  ---> general information --> memory initialization ---> lines search ---> timestamps search  ---> sorting END
//! Information about timestamp column is taken from user. 
    
    timestamp_col = *timestamp_column;
    timestamp_len = 0;
    if(csv_import(path_to_csv) == DONE){
                if(csv_info(csv) == DONE){
                    if( csv_get_lines(csv)  == DONE && csv_get_timestamps(csv) == DONE){
                        if(sort_timepointers(timestamps, lines) == DONE ){
                            #ifdef TEST_CASE
                            general_check(1);
                            #endif
                            return &lines;
                        }
                    }
                }
            }
        return NULL;
}




#endif
