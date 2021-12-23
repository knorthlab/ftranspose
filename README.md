# ftranspose

Quickly transpose a large delimited file

## Installation

The source code was written in the C99 standard, which may need to be specified to your compiler, e.g. `--std=c99`  

## Usage

``` 
ftranspose [ -i input ] [ -o output ] [ -d delim ] [ -D delim ]
```

By default, `ftranspose` reads and write from standard input/output, and delimiters are set to the TAB character `\t`.

### Examples

* Transpose tab-delimited `myfile.tsv` to tab-delimited `t_myfile.tsv`

  ```
  ftranspose -i myfile.tsv > t_myfile.tsv
  ```
  or
  ```
  ftranspose -i myfile.tsv -o t_myfile.tsv
  ```

* Transpose comma-delimited file `myfile.csv` into tab-delimited file `t_myfile.tsv`

  ```
  ftranspose -i myfile.csv -o t_myfile.tsv -d , -D \t
  ```

## Memory use

Since this program does not create intermediary files, there must be sufficient memory allocated to load the entire input file
