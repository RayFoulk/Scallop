routine fibonacci
  print "limit is {%1}"

  assign i 0
  assign j 1
  assign k 1

  while ({k} < {%1})
    assign k ({i} + {j})
    print "{k}"

    assign i {j}
    assign j {k}
  end
end

fibonacci 1000
