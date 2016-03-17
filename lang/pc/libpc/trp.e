#

; $Id$
;
; (c) copyright 1983 by the Vrije Universiteit, Amsterdam, The Netherlands.
;
;          This product is part of the Amsterdam Compiler Kit.
;
; Permission to use, sell, duplicate or disclose this software must be
; obtained in writing. Requests for such permissions may be sent to
;
;      Dr. Andrew S. Tanenbaum
;      Wiskundig Seminarium
;      Vrije Universiteit
;      Postbox 7161
;      1007 MC Amsterdam
;      The Netherlands
;
;

 mes 2,EM_WSIZE,EM_PSIZE

#define TRAP    0

; _trp() and trap() perform the same function,
; but have to be separate. trap exists to facilitate the user.
; _trp is there for the system, trap cannot be used for that purpose
; because a user might define its own Pascal routine called trap.

; _trp is called with one parameter:
;       - trap number (TRAP)

 exp $_trp
 pro $_trp,0
 lol TRAP
 trp
 ret 0
 end ?