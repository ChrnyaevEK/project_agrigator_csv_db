# Timescaledb
## Timescaledb provides importing csv file, sorting by timestamp (fresh data goes first) and writing sorted data to database along with response by HTML table of sorted data. 
Author: **Cherniaev Egor**
#
To perfom all functions properly, user should have following informations

1. Path to csv file
2. Number of column were timestamp is presented
3. Database connection data([`connetion string`](https://www.postgresql.org/docs/10/libpq-connect.html)). Example `"host=localhost dbname=your_database user=your_user password=your_password"`. Only this information is used to connect database.
4. Table name

After data has correctly been supplied, work begins. In other cases, an error occurs and error description is returned (more on that later).

## Requirements (for server)
- [Kore.io framework](https://kore.io/)
- [Postgresql](https://www.postgresql.org/)

## How to use?
To **run kore application**  use `kodev clean && kodev build && kodev run` inside of main directory 

Then, all you need is to collect all needed information and then perform GET HTTP request.

**Collect :** path/to/file.csv, my_column_with_timestamp,  my_table_name.

**Do :**`http://host_server:port/?pth=path%2Fto%2Ffile.csv&ts=my_column_with_timestamp&tbl=my_table_name`

Request should contain following query **parameters**: 
- **pth** -  path to file to perform sorting on,file should exist
- **ts** - timestamp column number, wich is used to identify timestamp(**column are counted from 0**)
- **tbl** - table name.

All with URL encoding. If the table doesn't exist - *it will be created* with the name you have used. After the request was received, a sorting process begins. In case of fail, an error report is returned and the user may try to change some supplied information. General errors are described below. In this version it is impossible to change the database connection string without code changing or to change timestamp format. All types of timestamp are available. The only restriction is- they should follow [ISO 8601](https://en.wikipedia.org/wiki/ISO_8601) to perform sorting correctly. If the file is "broken", some unexpected errors may occur.

## General activity description
The file gets loaded and timestamps are found. Then goes sorting. Sorting uses [the combo sort algorithm](https://en.wikipedia.org/wiki/Comb_sort). After sorting, timestamps are ordered this way: **fresh data first** (before 2001 2004 1900 2017 ---> after 2017 2004 2001 1900). As it has been finished, database query is built and performed. In case of success, the HTML table is built and the program will send it as the response. 

## Possible errors (errors explanation)
`_The response could not be created correctly_` : Means that an internal error happened. Try again or check csv file you are working with.

`_Error in querying SQL_` : Means that connection to database for some reasons was not accepted. Check connection string or your rights related to this database

`_Error in creating table_` : Table from file could not be created(header in file or data in file are not acceptable for database)

`_Error while sorting CSV(check file or parameters)_`: The most common and possible error. Means that file contains error in format or that you have entered wrong timestamp column number. What is considered as error? First of all - if file does not exist. Then - timestamps have different length, lines have different number of columns(it does not match header). "Broken files" this cause error too.

`_Error in parameters_` : Wrong identifiers used(pth-->path-->error) and/or data does not fit filter(e.i. ts-timestamp column number- contains anything else but decimal number). Some additional info could be returned, to expand error.

> **CHECK BEFORE USE**: timestamp_col(http:ts), path_to_csv(http:pth). FOR DATABASE : table_name(http:tbl), max_entry_size(in code)


