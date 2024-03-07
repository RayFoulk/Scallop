routine testwhile
  assign i 3
  while ({i} > 0)
    print "i is {i}"
    assign i ({i} - 1)
  end
end

testwhile

assign i 0
while ({i} < 3)
  print "i is {i}"
  assign i ({i} + 1)
end

print "all done"
