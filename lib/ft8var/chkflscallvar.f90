subroutine chkflscallvar(call_a,call_b,falsedec)

  character*12 call_a,call_b
  logical(1) falsedec

  if(len_trim(call_a).lt.0 .or. len_trim(call_b).lt.0) falsedec=.true.
  falsedec=.false.

  return
end subroutine chkflscallvar
