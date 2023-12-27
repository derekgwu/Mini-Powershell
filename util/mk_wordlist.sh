#!/bin/sh

# download a frequency-annotated word list, generate a word list with
# a separate word listed corresponding to frequency (/1000 to make the
# list not 0.5GiB), shuffle (randomize) the list to test the
# implementation better, and output to stdout.
#
# Note that http://www.kilgarriff.co.uk/bnc-readme.html gives an
# overview of the data format.
wget http://www.kilgarriff.co.uk/BNClists/lemma.al 2> /dev/null
cat lemma.al | awk '{for (i = 0; i < $2; i+=1000) {print $3;}}' | shuf
# clean up
rm lemma.al
