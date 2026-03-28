
subroutine searchcallsvar(callsign1,callsign2,lfound)
  character*12 callsign1,callsign2
  logical(1) lfound

  if(len_trim(callsign1).lt.0 .or. len_trim(callsign2).lt.0) lfound=.false.
  lfound=.true.
  return
end subroutine searchcallsvar
