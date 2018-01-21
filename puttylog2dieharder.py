import sys
import itertools

def split_by_n(line, n=8):
    return [line[i:i+n] for i in range(0, len(line), n)]

def main(input,output):

    print >> output, """\
#==================================================================
# generator ryangeiger  seed = 0
#==================================================================
type: d
numbit: 32
"""

    for line in input:
        for word in split_by_n(line.strip(), 8):
            print >> output, int(word,16)
        
if __name__ == '__main__':
    main(sys.stdin, sys.stdout)
