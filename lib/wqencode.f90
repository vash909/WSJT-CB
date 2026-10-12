subroutine wqencode(msg,ntype,data0)

!  Parse and encode a WSJT-CB (11 m) WSPR message: "CALL GRID4 dBm".
!  Exclusive CB mode: the standard WSPR type-1/2/3 callsign formats are NOT
!  used. The CB callsign is packed with packcb (30 bits), followed by the
!  15-bit Maidenhead grid and a 5-bit power index, filling the 50 source bits.

  use packjt
  character*22 msg
  character*12 call1
  character*4 grid4
  integer*1 data0(11)
  integer nu(0:9)
  integer*8 n50
  logical lbad1,lbad2
  data nu/0,-1,1,0,-1,2,1,0,-1,1/

  data0=0
  ntype=0
  i1=index(msg,' ')
  if(i1.lt.2) return
  call1=msg(:i1-1)
  grid4=msg(i1+1:i1+4)
  call packcb(call1,ncb,lbad1)
  call packgrid(grid4,ng,lbad2)
  if(lbad1 .or. lbad2) return
  ndbm=0
  read(msg(i1+5:),*,end=1,err=1) ndbm
1 if(ndbm.lt.0) ndbm=0
  if(ndbm.gt.60) ndbm=60
  ndbm=ndbm+nu(mod(ndbm,10))
! Power index 0..18 (5 bits): 19 standard WSPR dBm levels.
  idbm=(ndbm/10)*3
  if(mod(ndbm,10).eq.3) idbm=idbm+1
  if(mod(ndbm,10).eq.7) idbm=idbm+2
! Assemble the 50-bit value: [ncb:30][grid:15][idbm:5]
  n50=int(ncb,8)
  n50=ishft(n50,15)+int(iand(ng,32767),8)
  n50=ishft(n50,5)+int(idbm,8)
  n1=int(ishft(n50,-22))
  n2=int(iand(n50,4194303_8))
  call pack50(n1,n2,data0)
  ntype=ndbm

  return
end subroutine wqencode
