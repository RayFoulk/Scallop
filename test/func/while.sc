routine testwhile			# construct. should exec to push
  assign i 3				# regular. should linefunc to routine (in scope)
  while ({i} > 0)			# construct. should linefunc to routine AND exec to push
    print "i is {i}"		# regular. should linefunc to routine
    assign i ({i} - 1)		# regular. should linefunc to routine
  end						# construct. should linefunc to routine AND exec to pop
end							# construct. should NOT linefunc SHOULD exec to pop

testwhile					# newly registered regular.  should exec routine immediately

assign i 0					# regular. should exec immediately
while ({i} < 3)				# construct. should exec to push
  print "i is {i}"			# regular. should linefunc to while loop
  assign i ({i} + 1)		# regular. should linefunc to while loop
end							# construct. should exec to pop (and run while loop)

print "all done"			# regular. should exec immediately


########################################################################
# TRUTH TABLE
#
# within_routine_decl  end_of_routine_decl  cmd_is_construct  should_linefunc  should_exec
# within_construct???  end_of_construct???
#
#         F                    F                    F                NO            YES
#         F                    F                    T                NO            YES
#         F                    T                    F            -- ERROR: UNMATCHED END --              ?
#         F                    T                    T            -- ERROR: UNMATCHED END --
#         T                    F                    F                YES           NO
#         T                    F                    T                YES           YES
#         T                    T                    F                NO?           NO?
#         T                    T                    T                NO            YES
