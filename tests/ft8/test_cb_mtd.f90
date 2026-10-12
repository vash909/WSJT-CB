program test_cb_mtd
  use cb_callsigns
  use packjt77, only: pack77
  use packjt77var, only: unpack77var
  use ft8_mod1, only: mycall,hiscall
  implicit none
  character(len=12), parameter :: valid_calls(*) = [character(len=12) :: &
    '1A1','1AT106','26AT101','161XZ085','001AB123','1AT1000','999ZZ/ZZ']
  character(len=12), parameter :: invalid_calls(*) = [character(len=12) :: &
    '','26AT','K1ABC','26AT1000','1AT?','1234AT1','1ABC1','1AT/ABC','1AT/A','<...>']
  character(len=37) :: msg,decoded
  character(len=77) :: bits
  integer :: i,j,i3,n3,bad
  logical :: ok
  mycall='1AT106'
  hiscall='26AT101'
  call fillhashvar(1,.false.)
  call fillhashvar(1,.true.)
  do i=1,size(invalid_calls)
    if(is_complete_cb_callsign(invalid_calls(i))) stop 1
  enddo
  do i=1,size(valid_calls)
    if(.not.is_complete_cb_callsign(valid_calls(i))) stop 2
    do j=1,4
      select case(j)
      case(1)
        msg='CQ '//trim(valid_calls(i))
      case(2)
        msg='<1AT106> '//trim(valid_calls(i))
      case(3)
        msg='<1AT106> '//trim(valid_calls(i))//' RR73'
      case(4)
        msg=trim(valid_calls(i))//' <26AT101>'
      end select
      if(.not.is_cb_type4_message(msg)) stop 3
      i3=-1
      n3=-1
      call pack77(msg,i3,n3,bits)
      call unpack77var(bits,1,decoded,ok,1)
      if(.not.ok .or. decoded/=msg) then
        print *, 'round trip failed: ',msg,' -> ',decoded,ok,i3
        stop 4
      endif
      bad=0
      call chkfalse8var(decoded,i3,n3,bad,0,.false._1)
      if(bad/=0) stop 5
      ! This exception must never revive a CRC/quality failure.
      bad=1
      call chkfalse8var(decoded,i3,n3,bad,0,.false._1)
      if(bad/=1) stop 6
    enddo
  enddo
  if(is_cb_type4_message('CQ 26AT')) stop 7
  if(is_cb_type4_message('<...> <...>')) stop 8
  if(is_cb_type4_message('<1AT106> 26AT101 GARBAGE')) stop 9
  if(is_cb_type4_message('CQ 26AT101 EXTRA')) stop 10
  ! Preserve the original rejection of implausible amateur-style type 4 calls.
  msg='CQ 53HDFKJEASD'
  i3=-1
  n3=-1
  call pack77(msg,i3,n3,bits)
  call unpack77var(bits,1,decoded,ok,1)
  if(ok) stop 11
  print *, 'CB type 4 packing, unpacking and plausibility checks passed'
end program
