! Keep this grammar in sync with Radio::is_complete_cb_callsign.
! These predicates replace amateur-only plausibility checks, never CRC checks.
module cb_callsigns
  implicit none
  private
  public :: is_complete_cb_callsign, is_cb_type4_message
contains
  pure logical function is_complete_cb_callsign(text) result(valid)
    character(len=*), intent(in) :: text
    character(len=len(text)) :: call
    integer :: i,n,prefix,letters,suffix,k
    valid=.false.
    call=trim(adjustl(text))
    n=len_trim(call)
    do i=1,n
      k=iachar(call(i:i))
      if(k>=iachar('a') .and. k<=iachar('z')) call(i:i)=achar(k-32)
    enddo
    i=1
    do while(i<=n)
      if(call(i:i)<'0' .or. call(i:i)>'9') exit
      i=i+1
    enddo
    prefix=i-1
    if(prefix<1 .or. prefix>3) return
    do while(i<=n)
      if(call(i:i)<'A' .or. call(i:i)>'Z') exit
      i=i+1
    enddo
    letters=i-prefix-1
    if(letters<1 .or. letters>2 .or. i>n) return
    if(call(i:i)=='/') then
      if(n-i/=2) return
      valid=all_letters(call(i+1:n))
    else
      suffix=n-i+1
      if(suffix>3 .and. .not.(prefix==1 .and. suffix==4)) return
      do k=i,n
        if(call(k:k)<'0' .or. call(k:k)>'9') return
      enddo
      valid=.true.
    endif
  end function

  pure logical function all_letters(text) result(valid)
    character(len=*), intent(in) :: text
    integer :: i
    valid=.false.
    do i=1,len(text)
      if(text(i:i)<'A' .or. text(i:i)>'Z') return
    enddo
    valid=.true.
  end function

  pure logical function hashed_cb(text) result(valid)
    character(len=*), intent(in) :: text
    integer :: n
    n=len_trim(text)
    valid=text=='<...>'
    if(n<3) return
    if(text(1:1)=='<' .and. text(n:n)=='>') then
      valid=valid .or. is_complete_cb_callsign(text(2:n-1))
    endif
  end function

  pure logical function is_cb_type4_message(text) result(valid)
    character(len=*), intent(in) :: text
    character(len=len(text)) :: first,second,tail
    integer :: i,j
    valid=.false.
    i=index(trim(text),' ')
    if(i<2) return
    first=text(:i-1)
    tail=adjustl(text(i+1:))
    if(first=='CQ') then
      valid=is_complete_cb_callsign(tail)
      return
    endif
    j=index(trim(tail),' ')
    second=tail
    if(j>0) then
      second=tail(:j-1)
      tail=adjustl(tail(j+1:))
      if(tail/='RRR' .and. tail/='RR73' .and. tail/='73') return
    endif
    ! A type 4 QSO carries exactly one full call and one hashed call.
    valid=(is_complete_cb_callsign(first) .and. hashed_cb(second)) .or. &
          (hashed_cb(first) .and. is_complete_cb_callsign(second))
  end function
end module
