
//! \author Cherniaev Egor 
#include <kore/kore.h>
#include <kore/http.h>
#include <kore/pgsql.h>
#include <libintl.h>
#include <locale.h>
#define _(STRING) gettext(STRING)
//! Library to parse and sort csv by path
//! CHECK BEFORE USE : timestamp_col(http:ts), path_to_csv(http:pth). FOR DATABASE : table_name(http:tbl), max_entry_size(in code)
#include "agrlib_v2.h"
//! Define TEST_CASE to perfom output to stdout when debuging or testing
#define TEST_CASE
#define _(A) gettext(A)
//! For l18n - define here the directory with .mo files
#define LOCALE_DIR "locale"
//! Define here the name of file.mo to use
#define PACKAGE "core"

// =======================================================================================================
//! Tamplate for generated HTML table
char        html_table[] = "<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><style>table{font-family: arial, sans-serif;border-collapse: collapse;width: 100%;}td, th {border: 1px solid #dddddd;text-align: left;padding: 8px;}tr:nth-child(even) {background-color: #dddddd;}</style></head><body><h2>RESULTS</h2><table>";
//! Tamplate for generated HTML error response
char        error_response_header[] = "<!DOCTYPE html><html><head><meta charset=\"utf-8\"></head><body>LOG:";
char        error_response_end[] = "</body></html>";
//! End of table
char        html_table_end[] = "</table></body></html>";
//! Main connection string, fill with your parametrs according to PGSQL connection string rules 
char        conn_str[] = "host=localhost dbname=your_database_name user=example_user password=example";
//! Table name to use if it is not created from file name(CREATE_NAME_FROM_FILE not defined)
char        *table_name;
//! If table is new created, each entry is treated as text(varchar). Here is max size of each entry
char        max_entry_size[] = " varchar(40) ";
//! This key is used for column with timestamp, if database is created
char        ts_in_db[] = " timestamp ";
char                *path_to_csv;
//! Buffer for possible errors. Is included in response in case of fail. Errors are collected  from very deep, but not traced
static struct kore_buf* errors;

//! Holds the number of attempts to do, when trying to access database
int         max_try_conn = 5;
//! This flag wraps query in $$ for escaping some characters(like quots)
int         escape_needed = 1;
//! If no name was suplied to create table with and this flag is up - we will get name from file
int         form_name_from_path = 1; 
int         name_len = 0;

// =======================================================================================================
//! Register database
int			init(int state);
//! Dealing with request
int			page(struct http_request *); 
//! Forms html response, but does not send it. Table is formed from 'table_sorted'
int         form_response ( struct kore_buf*  buf , char **table_sorted ) ; 
//! Responsible for quering database with INSERT commands
int         form_and_fire_query( struct kore_buf*  qr_buf ,struct kore_pgsql* sql,char *line );
//! Sets up / opens connection with database 
int         setup_conn(struct kore_pgsql* sql); 
//! Creates table if not exist
int         create_table_if_notexist(char *csv_header,struct kore_pgsql* sql ); 
//! Sets up database connection, performs all queries 
int         database_query(struct kore_pgsql *sql,
                            char **table_sorted ) ;

// General : all functions return KORE_RESULT_OK in case of success or KORE_RESULT_ERROR in case of fail


int
init(int state)
{

	//! Register our database
	kore_pgsql_register("db", conn_str);
    //! Setting the i18n environment. L18n is done by user's settings 
    setlocale (LC_MESSAGES, "");
    bindtextdomain (PACKAGE, LOCALE_DIR);
    textdomain (PACKAGE);
}

int page(struct http_request *req){

    req->status = HTTP_STATUS_INTERNAL_ERROR;
    errors = kore_buf_alloc(25);
    kore_buf_appendf(errors, "%s", error_response_header);
    //! The path to file is passed in request with tag 'pth'.
    //! Timestamp column has tag 'ts'.
    unsigned short int s_ts_col = 0;
    /*
     * Before we are able to obtain any parameters given to
     * us via the query string we must tell Kore to parse and
     * validate them.
     *
     * NOTE: All parameters MUST be declared in a params {} block
     * inside the configuration for Kore! Kore will filter out
     * any parameters not explicitly defined.
     *
     * See conf/main.conf on how that is done, this is an
     * important step as without the params block you will never
     * get any parameters returned from Kore.
     *
     */
    //! Validate  suplied parameters
    http_populate_get(req);
    http_argument_get_string( req, "tbl", &table_name);
    http_argument_get_string( req, "pth", &path_to_csv);
    if (http_argument_get_string( req, "pth", &path_to_csv) && 
        http_argument_get_uint16(req, "ts", &s_ts_col) && 
        http_argument_get_string( req, "tbl", &table_name)) { 
        char *tmp = table_name;
        for(int i = 0; i < 63; i ++){
            if(*(tmp++) == '\0'){name_len = i; break;}
        }

        //! Holds csv, sorted by agregate()
        //! Sorted csv is a collection of pointers in rigth order, the header is included
        char ***sorted_csv = agregate(path_to_csv, &s_ts_col); 
        //! Holds all errors we will find, to include in response
        //! Sorted succesfuly ? go for database.
        //! Note that first we perfom quering, and only then responsing
        if(sorted_csv != NULL){     
            //! Database workflow done in usual fasion 
            struct kore_pgsql sql;
            /*!
            Logic : try to create table(even if exists) ---> if exists : skip, else : create table with eather 
            suplied name or name formed from file path
            */
            if (create_table_if_notexist(*lines, &sql) == KORE_RESULT_OK){ 
                //! When done, we will form and send our queries with csv data
                if (database_query(&sql, *sorted_csv) == KORE_RESULT_OK){
                    //! Holds response HTML table as text
                    struct kore_buf* resp = kore_buf_alloc(max_line_len); 
                    //! Forming response
                    if (form_response(resp, *sorted_csv) == KORE_RESULT_OK){ 
                        //! For test purposes
                        #ifdef TEST_CASE
                        kore_log(LOG_NOTICE, "----OK -----");
                        #endif
                        //! As we are here this means everything is ok, no errors met and we are ready to finally response with success 
                        req->status = HTTP_STATUS_OK; //! everything is ok
                        http_response(req, 200, resp -> data, resp -> offset);
                        return (KORE_RESULT_OK);
                    }else{ 
                        //! An error in forming HTML table occured. We are going to inform user about it
                        //! Note, when changing error text - change it's length
                        kore_buf_appendf(errors,"%s" , _("_The response could not be created correctly_"));
                        #ifdef TEST_CASE
                        kore_log(LOG_NOTICE, "----error forming response  -----");
                        #endif
                    }
                }else{
                    //! An error in querring database occured. It can be what ever, other includes in error report will try to tell more on this
                     kore_buf_appendf(errors,"%s", _("_Error in querring SQL_"));
                     #ifdef TEST_CASE
                     kore_log(LOG_NOTICE, "----error querring sql-----");
                     #endif
                }
            }else{
                //! An error in creating table occured. 
                kore_buf_appendf(errors, "%s" ,_("_Error in creating table_"));
                #ifdef TEST_CASE
                kore_log(LOG_NOTICE, "----error creating table-----");
                #endif
            }
        }else{
            //! An error in sorting csv occured:
            //! Generaly it heppens due to mistakes in 1) creating csv 2) mistakes with timestamp length and column
            kore_buf_appendf(errors, "%s",_("_Error while sorting CSV(check file or parameters_"));
            #ifdef TEST_CASE
            kore_log(LOG_NOTICE, "----error sorting -----");
            #endif
        }
    }else{
            //! An error in sorting csv occured:
            //! Generaly it heppens due to mistakes in 1) creating csv 2) mistakes with timestamp length and column
            kore_buf_appendf(errors, "%s", _("_Error in parameters_"));
            #ifdef TEST_CASE
            kore_log(LOG_NOTICE, "----error parameters-----");
            #endif
    }
    //! We dont get here if everything is OK
    kore_buf_appendf(errors, "%s", error_response_end);
    http_response(req, 500, errors -> data, errors -> offset);
    return (KORE_RESULT_OK);
}

/*!
 *The table is created and filled with csv data. Some style can be added.
 *First it takes header and then adds each piece od data wraped properly with table tags
 *Then end part is added.
 */
int form_response ( struct kore_buf*  buf , char **table_sorted ){
    //! Header
    kore_buf_append(buf, &html_table , sizeof(html_table)/sizeof(char));
    char current_char;
    char  *tmp;
    //! Indicates if open tag has been added
    int open_tag = 0;
    //! Indicates if close tag has been added
    int close_tag = 0;
    //! Formating each line and append it to buffer
    for(size_t i = 0; i < rows; i++){
        //! tmp holds pointer to the next sorted line
        tmp = *(table_sorted++);
        //! Create new line
        kore_buf_append(buf, "<tr>", 4); 
        //! As we have counted maximum line length, we do not need to iterate more
        for(size_t f = 0; f <= max_line_len; f++){ 
            //! Now we are looking for two things : end of line and separators 
            current_char = *tmp; 
            if(current_char != '\0'){
                if(current_char != sep){
                    //! Rutine for tags
                    if(! open_tag){
                        kore_buf_append(buf, "<th>", 4);
                        open_tag = 1;
                        }
                    //! Append data
                    kore_buf_append(buf, tmp , 1); //! = &current_char
                }else{
                    kore_buf_append(buf, "</th>", 5);
                    close_tag = 1;
                    open_tag = 0;
                }
            }else{
                kore_buf_append(buf, "</th>", 5);
                break;
                }
        tmp++;
        }
        open_tag = 0;
        close_tag = 0;
        kore_buf_append(buf, "</tr>", 5);
    }
    //! As we are done with tags - close HTML document
    kore_buf_append(buf, &html_table_end ,sizeof(html_table_end)/sizeof(char));
    return KORE_RESULT_OK;
}


   /*! 
    * Each line of csv sent separetly by this function. If escape is needed - this function wraps query with escape characters
    * As input :
    *    qr_buf, extern buffer to temp.hold query
    *    sql, to post query to
    *    line, separated line to fire off
    */
int form_and_fire_query( struct kore_buf*  qr_buf ,struct kore_pgsql* sql,char *line ){
    //! Postgres uses dollar sign to escape symbols like ', " ...
    //! *_e stands for escape, and *_s for simple, with no escape 
    char entry_open_e[] = " $$";
    char entry_close_e[] = " $$,";
    char entry_end_e[] = "$$ );";
    char entry_open_s[] = "'";
    char entry_close_s[] = "',";
    char entry_end_s[] = "');";
    // Will hold propriate tags
    char *entry_open;
    char *entry_close;
    char *entry_end;
    //! Holds sizes of query tags, so not to count them each time
    int sizes[3]  = {1,2,3};
    //! Depending on settings we choose tags
    if(escape_needed){
        entry_open = entry_open_e;
        entry_close = entry_close_e;
        entry_end = entry_end_e;
        //! Escape sizes differes by only two
        sizes[0] +=2;
        sizes[1] +=2;
        sizes[2] +=2;
    }else{
        entry_open = entry_open_s;
        entry_close = entry_close_s;
    }
    //! Form and send query to previously initialized database
    //! Note sizes of command
    kore_buf_append(qr_buf,"INSERT INTO ", 12);
    //! Due to EOL we should down it by 1
    kore_buf_append(qr_buf, table_name, name_len); 
    kore_buf_append(qr_buf, " VALUES (", 9);
    //! Holeds current symbol
    char current_char;
    //! Counter for open apostrofs
    int open_ap = 0; 
    //! Counter for close apostrofs
    int close_ap = 0;
        for(size_t f = 0; f <= max_line_len; f++){
            current_char = *line;
            if(current_char != '\0'){
                if(current_char != sep){
                    if(! open_ap){
                        //! Appending open tags
                        kore_buf_append(qr_buf, entry_open, sizes[0]); 
                        open_ap = 1;
                        }
                //! Appending data
	            kore_buf_append(qr_buf, line , 1);
	    }else{
            //! Appending close tag
	        kore_buf_append(qr_buf,entry_close, sizes[1]); 
	        close_ap = 1;
	        open_ap = 0;}
	}else{
        //! Appending end tag
    	kore_buf_append(qr_buf,entry_end, sizes[2]);
        break;}
        line++;
        }
    //! Firing off built query
	if (kore_pgsql_query(sql, kore_buf_stringify(qr_buf, NULL)) != KORE_RESULT_OK) {
            #ifdef TEST_CASE
            kore_pgsql_logerror(sql);
            #endif
            kore_buf_appendf(errors, "%s", _("_Check data format or input data_"));
        	return (KORE_RESULT_ERROR);
	}

            //! As the buffer is always in use - clean it as soon as we are done
            kore_buf_cleanup(qr_buf);
        	return (KORE_RESULT_OK);

    }
/*!
 * If table does not exist, we create it
 * This is reached with SQL command
 *      csv_header, csv header to init table with
 *      sql, database to use
 */
int create_table_if_notexist(char *csv_header,struct kore_pgsql* sql ){
    //! Buffer to hold query
    struct kore_buf* qr_buf = kore_buf_alloc(max_line_len);
    //! We would like to check if table exists, before creating it
    kore_buf_append(qr_buf,"CREATE TABLE IF NOT EXISTS ", 27 );
    kore_buf_append(qr_buf,table_name,name_len); 
    kore_buf_append(qr_buf," (", 2); 
    //! Filling table header in form_and_fire_query fasion 
    char current_char;
    //! As well we are looking for timestamp column, to indicate it as timestamp
    int col_count = 0;
    int open_ap = 0;
    int close_ap = 0;
        for(size_t f = 0; f <= max_line_len; f++){
            current_char = *csv_header;
            if(current_char != '\0'){
                if(current_char != sep){
                    kore_buf_append(qr_buf, csv_header , 1);
                }else{
                    if(col_count == timestamp_col){
                        kore_buf_append(qr_buf,ts_in_db, sizeof(ts_in_db) -1 );
                    }else{
                        kore_buf_append(qr_buf,max_entry_size, sizeof(max_entry_size) -1 );
                    }
                    col_count++;
                    kore_buf_append(qr_buf, ",", 1);
                    close_ap = 1;
                    open_ap = 0;}
            }else{
                if(col_count == timestamp_col){
                    kore_buf_append(qr_buf,ts_in_db, sizeof(ts_in_db) -1 );
                }else{
                    kore_buf_append(qr_buf,max_entry_size, sizeof(max_entry_size) -1 );
                }
                kore_buf_append(qr_buf, ");", 3);
                break;}
                csv_header++;
        }
    //! Setting up connection
    if(setup_conn(sql) == KORE_RESULT_ERROR){
        return KORE_RESULT_ERROR;
    }
    //! Performing query
    if (kore_pgsql_query(sql, kore_buf_stringify(qr_buf, NULL)) == KORE_RESULT_ERROR) {
        #ifdef TEST_CASE
        kore_pgsql_logerror(sql);
        #endif
        kore_pgsql_cleanup(sql);
        kore_buf_appendf(errors,"%s", _("_Creation failed, check your rigths_"));
        return (KORE_RESULT_ERROR);
    }
    kore_pgsql_cleanup(sql);
    return (KORE_RESULT_OK);
}

    /*
     * Initialise our kore_pgsql data structure with the database name
     * we want to connect to (note that we registered this earlier with
     * kore_pgsql_register()). We also say we will perform a synchronous
     * query (KORE_PGSQL_SYNC). If we do not get it at first time, we will 
     * try up to max_try_conn.
     */
int setup_conn(struct kore_pgsql* sql){
	//! Initialise data structure
    kore_pgsql_init(sql); 
	if (kore_pgsql_setup(sql, "db", KORE_PGSQL_SYNC) == KORE_RESULT_ERROR ){
		//! Try again
        int ok = 0; 
		for(int i = 0; i < max_try_conn; i++){
		    if (kore_pgsql_setup(sql, "db", KORE_PGSQL_SYNC) == KORE_RESULT_OK){ok = 1; break;}
        }
        //! All  attempts failed
        if(! ok ){ 
            #ifdef TEST_CASE
            kore_pgsql_logerror(sql);
            #endif
            kore_buf_appendf(errors, "%s", _("_Connection failed, check connection data_") );
            kore_pgsql_cleanup(sql);
            return (KORE_RESULT_ERROR);
        }
    }
    return KORE_RESULT_OK;
}

    /*
     * Gets all lines through forming query
     * and sending it. Sets up buffers.
     */

int database_query(struct kore_pgsql *sql,  char **table_sorted ) 
{
    //! Open connection
    setup_conn(sql); 
    //! Prepare memory for queries, initialised olny onece
	struct kore_buf* qr_buf = kore_buf_alloc(max_line_len); 
	//! Forming and firing off queries
	for(size_t j = 0; j < rows - 1; j++ ){
        //! As we do not include first row (it is a header)
        table_sorted++; 
        if(form_and_fire_query(qr_buf , sql ,*table_sorted) != KORE_RESULT_OK){
            #ifdef TEST_CASE
            kore_log(LOG_NOTICE, "\n   QUERING FAILED WITH LINE num.%li    \n", ++j);
            #endif
            kore_buf_appendf(errors,"%s %li_", _("_QUERING FAILED WITH LINE num."), j);
            kore_pgsql_cleanup(sql);
            return KORE_RESULT_ERROR;
        }
	}
        kore_pgsql_cleanup(sql);
        return (KORE_RESULT_OK);
}
















