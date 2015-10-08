/*
 * ftranspose.c
 * Author: Chris Wood <chrislwood@gmail.com>
 *
 * LICENSE: freely distribute, modify, compile... please retain authorship
 *
 * v1.0 - Chris Wood - Initial
 * v1.1 - Chris Wood - fix issue where last line char is a delimiter
 * v1.2 - Chris Wood - add check for field length overrun
 * v1.3 - Chris Wood - allow custom element width (-f option)
 *      - ask for less memory if realloc fails
 
 * v1.4 - Antoine Baldassari: baldassa@email.unc.edu
 *      - allows piping 
 *      - truncates data elements to fit size limit
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define ARG_STR_LEN            512
#define DEFAULT_FIELD_LENGTH   20
#define BACKSLASH 92
#define TAB 9

#define VERSION_STR   "1.3"

typedef struct {
    int  element_size;
    int  verbosity;
    char in_delim;
    char out_delim;
    char in_filename[ ARG_STR_LEN ];
    char out_filename[ ARG_STR_LEN ];
} args_t;
static args_t args;

typedef long int idx_t;

typedef struct {
  idx_t rows;              /* # rows in matrix                                        */
  idx_t cols;              /* # columns in matrix                                     */
  idx_t element_capacity;  /* maximum # of elements that can be stored in data buffer */
  idx_t element_count;     /* current # of elements stored in data buffer             */
  idx_t pos;               /* cursor for writing new elements to data buffer          */
  int   element_size;      /* # of bytes in each data element (fixed width)           */
  char *data;              /* data buffer                                             */
  idx_t bytes_allocated;   /* metrics; total amount of RAM used                       */
}array_t;



void usage( int rc )
{
    fprintf( stderr, "\nftranspose OPTIONS\n"                                         \
                     "  Author:  Chris Wood\n"                                        \
                     "  Version: " VERSION_STR "\n\n"                                 \
                     "  - transpose a text file of rows/columns\n"                    \
		     " OPTIONS\n"                                                     \
		     "   -h                     help (this)\n"                        \
		     "   -v #                   verbosity (default=0)\n"              \
		     "   -d delim               input delimiter\n"                    \
		     "   -D delim               output delimiter\n"                   \
		     "   -f #                   field width (default %d chars)\n"     \
		     "   -i filename            input filename\n"                     \
		     "   -o filename            output filename\n\n",
             DEFAULT_FIELD_LENGTH  );
    exit( rc );
}

void free_array( array_t *a )
{
    if ( a == (array_t *)0 )
        return;
    free( (void *)a->data );
    free( (void *)a );
    return;
}

inline void insert_element( array_t *a, char *e )
{
    int new_elements = 0;

    /* 'a' might not be big enough to hold this next element.
     * If it isn't, then realloc.
     */
    if ( a->element_count >= a->element_capacity )
    {
        /* not big enough, re-allocate a huge chunk */
        array_t b;

        /* double current elements or start with a reasonable number
         * if first time through
         */
        if ( (new_elements = a->element_capacity) == 0 )
            new_elements = (4096 / a->element_size);

        /* if the huge chunk allocate fails this loop will cut down on the
         * requested bytes.  Ex. Imagine you've allocated 64MB.  The next time
         * we allocate data we'd request 128MB.  That might fail and if we
         * only needed +1MB (65MB) then this loop will allow the program
         * to continue */
        while( 1 )
        {
            /* try for double what we already have allocated */
            size_t size_bytes = a->element_size * (a->element_capacity + new_elements);

            /* ensure we allocate an integer multiple pages (4096 bytes) */
            size_bytes = ((4095 + size_bytes) >> 12) << 12;

            if ( args.verbosity >= 3 )
            {
                printf( "FAILED\nattempting realloc( %ld ) .. ", size_bytes );
                fflush(NULL);
            }

            /* try to grab more memory */
            b.data = (char *)realloc( (void *)a->data, size_bytes );

            /* if the realloc fails, adjust the requested amount and try again */
            if ( b.data == (char *)0 )
            {
                /* each failed alloc results in a 50% reduction in extra requested
                 * space until we hit the lower limit of 1 page, 4096 bytes.
                 */
                new_elements >>= 1;
                if ( new_elements == 0 )
                {
                    fprintf( stderr, "\nfailed to realloc in %s\n", __func__ );
                    free_array( a );
                    exit( EXIT_FAILURE );
                }

                /* go back to top of loop to retry the alloc with
                 * less memory requested */
                continue;
            }

            /* realloc successful, break out of loop */
            a->bytes_allocated = size_bytes;
            break;
        }

        /* indicate successful reallocation */
        if ( args.verbosity >= 3 )
        {
            printf( "PASSED\n" );
            fflush(NULL);
        }

        /* update the main array */
        a->data = b.data;
        a->element_capacity += new_elements;
    }

    memcpy( &(a->data[ a->pos ]), e, a->element_size );
    a->pos += a->element_size;
    a->element_count++;
}

array_t *read_array( char delim, char *filename, int element_size )
{
    idx_t col;
    FILE *fp;
    array_t *a = (array_t *)0;
    int i;
    char *e;
    int c;

	if (filename[0] == '\0') {
		if ((fp = stdin) == NULL) usage(EXIT_FAILURE);
	}
	else  if ( (fp = fopen(filename, "r")) == (FILE *)0 )
    {
        perror( filename );
		return;
    }

    if ( args.verbosity >= 1 )
    {
        printf( "reading array ... " ); fflush(NULL);
    }
    a = calloc( 1, sizeof(array_t) );
    a->element_size = element_size;
    e = calloc( 1, a->element_size );
    col = 0;
    i = 0;
    while( (c = fgetc(fp)) != EOF )
    {
		/* seek to end of field if reached size limit*/
		if ( ! ( (c == delim) || (c == '\n') || (c == EOF) ) && i >= a->element_size )
		{
			fprintf( stderr, "element @[%ld,%ld] size exceeded\n", a->rows, col);
			while ((c = fgetc(fp)) != '\n' && c != EOF && c != delim);
			i = a->element_size - 1;
        }	
			
        /* end of a data element */
        if ( (c == delim) || (c == '\n') || (c == EOF) )
        {
            if ( i > 0 )
            {
                e[i] = '\0';

                /* insert element into array */
                insert_element( a, e );
                col++;
            }

            /* end of a row */
            if ( c == '\n')
            {	
                a->rows++;

                /* adjust maximum row length */
                if ( col > a->cols )
                    a->cols = col;

                /* print something helpful for large runs */
                if ( args.verbosity >= 2 )
                {
                    printf( "row=%ld\n", a->rows);
                    fflush(NULL);
                }

                /* reset column counter */
                col = 0;
            }

            /* reset data element char index */
            i = 0;
        }
		/* write to buffer */
        else e[i++] = c;
    }

    if ( args.verbosity >= 1 )
    {
        printf( "DONE\nread in %ld elements (r=%ld, c=%ld)\n", a->element_count, a->rows, a->cols );
    }

    fclose( fp );
    free( (void *)e );

    return a;
}

void write_array_transposed( array_t *a, char *filename, char delim )
{
    idx_t row, col;
    FILE *fp;
    char *buf = (char *)0;

    if ( a == (array_t *)0 )
        return;

	if (filename[0] == '\0') {
		if ((fp = stdout) == (FILE*)0) usage(EXIT_FAILURE);
	}
	else  if ( (fp = fopen(filename, "w")) == (FILE *)0 )
    {
        perror( filename );
		return;
    }

    buf = malloc( a->element_size + 1 );

    if ( args.verbosity >= 1 )
    {
        printf( "writing array transposed ... " );
		fflush( NULL );
    }

    for( col = 0; col < a->cols; col++ )
    {
        for( row = 0; row < (a->rows-1); row++ )
        {
            memcpy( (void *)buf,
                    (void *)&(a->data[ (row * a->cols + col) * a->element_size ]),
                    a->element_size );
            buf[ a->element_size ] = '\0';
			fprintf( fp, "%s%c", buf, delim );
        }
        memcpy( (void *)buf,
                (void *)&(a->data[ (row * a->cols + col) * a->element_size ]),
                a->element_size );
        buf[ a->element_size ] = '\0';
		fprintf( fp, "%s\n", buf );
        if ( args.verbosity >= 3 )
        {
            if ( ((col % 10000) == 0) && (col > 1 ) )
            {
                printf( "line=%ld\n", col );
                fflush( NULL );
            }
        }
    }
    fclose( fp );
    if ( args.verbosity >= 1 )
    {
        printf( "DONE\n" );
        fflush( NULL );
    }
    free( (void *)buf );
}

int main( int argc, char *argv[] )
{
    int c;
    array_t *a, *b;

    memset( (void *)&args, 0UL, sizeof(args_t));
	args.element_size = DEFAULT_FIELD_LENGTH;
    while( (c = getopt( argc, argv, "f:hd:D:i:o:v:" )) != -1 )
    {
        switch ( c )
        {
        case 'v':
            args.verbosity = atoi(optarg);
            break;
        case 'f':
            args.element_size = atoi(optarg);
            break;
	case 'd':
		if(strlen(optarg) == 1) args.in_delim = *optarg;
		if(strlen(optarg) > 1) 
			if(optarg[1] == 't' && *optarg == BACKSLASH) args.in_delim = TAB;
		if(!args.in_delim)
		{
			fprintf(stderr, "Error: invalid input delimiter: %s\n", optarg);
			usage( EXIT_FAILURE );
		}
		break;
	case 'D':
		if(strlen(optarg) == 1) args.out_delim = *optarg;
		if(strlen(optarg) > 1) 
			if(optarg[1] == 't' && *optarg == BACKSLASH) args.out_delim = TAB;
		if(!args.out_delim)
		{
			fprintf(stderr, "Error: invalid output delimiter: %s\n", optarg);
			usage( EXIT_FAILURE );
		}
		break;
        case 'i':
            strncpy(args.in_filename, optarg, ARG_STR_LEN);
            args.in_filename[ ARG_STR_LEN - 1 ] = '\0';
            break;
        case 'o':
            strncpy(args.out_filename, optarg, ARG_STR_LEN);
            args.in_filename[ ARG_STR_LEN - 1 ] = '\0';
            break;
        case 'h':
            usage( EXIT_SUCCESS );
            break;
        default:
            usage( EXIT_FAILURE );
            break;
        }
    }
    if(args.verbosity > 0 && !args.out_filename[0])
    {
	fprintf(stderr, " verbosity setting overriden to 0 to preserve stdout\n");
	args.verbosity = 0;
    }

	//if (args.in_filename[0] == '\0' && stdin == NULL) usage(EXIT_FAILURE);
	//if (args.out_filename[0] == '\0' && stdout == NULL) usage(EXIT_FAILURE);

    if ( args.verbosity >= 2 )
    {
        printf( "field width  = [%d chars]\n", args.element_size );
        printf( "in_delim     = [%c]\n", args.in_delim );
        printf( "out_delim    = [%c]\n", args.out_delim );
        printf( "in_filename  = [%s]\n", args.in_filename );
        printf( "out_filename = [%s]\n", args.out_filename );
    }

    a = read_array( args.in_delim, args.in_filename, args.element_size );
    write_array_transposed( a, args.out_filename, args.out_delim );

    if ( args.verbosity >= 1 )
    {
        printf( "Total RAM used: %ld bytes.\n", a->bytes_allocated );
        fflush( NULL );
    }

    free_array( a );

    return EXIT_SUCCESS;
}
