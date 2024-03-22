assign i 0             # regular. should exec immediately
while ({i} < 3)        # construct. should exec to push
  print "i is {i}"     # regular. should linefunc to while loop
  assign i ({i} + 1)   # regular. should linefunc to while loop
end                    # construct. should exec to pop (and run while loop)

print "all done"       # regular. should exec immediately
