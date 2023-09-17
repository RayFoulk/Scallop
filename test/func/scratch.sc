# Routine Functional Test

routine myroutine
  print i'm in your routine
  alias test help
  test
  unregister test
  help
end

routine another_routine
  print about to define another routine
  routine nested_routine
    print you should not see this right away
  end

  help
end

print calling routines...
myroutine arg1 arg2 arg3
another_routine argx argy argz



