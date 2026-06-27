' InverseTrigTest.bas
' Diagnostic test for casinf, catanf, casinhf, cacoshf
' Tests inverse functions directly and via round-trip
' Prints raw values so we can see exactly what the firmware returns
'
Option Default Integer
Option Base 0

Print "============================================="
Print " Inverse Complex Function Diagnostic"
Print "============================================="
Print

' --- Input: z = 0.5 + 0.5i ---
Dim z% = Math(C_CPLX 0.5, 0.5)
Print "Input z = "; Math(C_REAL z%); " + "; Math(C_IMAG z%); "i"
Print

' =============================================
' Test 1: C_ASIN directly and via round-trip
' =============================================
Print "--- C_ASIN ---"
Dim s% = Math(C_SIN z%)
Print "  sin(z)  = "; Math(C_REAL s%); " + "; Math(C_IMAG s%); "i"
Dim as1% = Math(C_ASIN s%)
Print "  asin(sin(z)) = "; Math(C_REAL as1%); " + "; Math(C_IMAG as1%); "i"
Print "  expected      = 0.5 + 0.5i"
Print

' Also test asin with a known purely real input
Dim zr% = Math(C_CPLX 0.5, 0.0)
Dim as2% = Math(C_ASIN zr%)
Print "  asin(0.5+0i)  = "; Math(C_REAL as2%); " + "; Math(C_IMAG as2%); "i"
Print "  expected       = "; Asin(0.5); " + 0i"
Print

' =============================================
' Test 2: C_ACOS directly and via round-trip
' (this one reportedly passes - included as control)
' =============================================
Print "--- C_ACOS (control - should pass) ---"
Dim c% = Math(C_COS z%)
Print "  cos(z)  = "; Math(C_REAL c%); " + "; Math(C_IMAG c%); "i"
Dim ac1% = Math(C_ACOS c%)
Print "  acos(cos(z)) = "; Math(C_REAL ac1%); " + "; Math(C_IMAG ac1%); "i"
Print "  expected      = 0.5 + 0.5i"
Print

' =============================================
' Test 3: C_ATAN directly and via round-trip
' =============================================
Print "--- C_ATAN ---"
Dim t% = Math(C_TAN z%)
Print "  tan(z)  = "; Math(C_REAL t%); " + "; Math(C_IMAG t%); "i"
Dim at1% = Math(C_ATAN t%)
Print "  atan(tan(z)) = "; Math(C_REAL at1%); " + "; Math(C_IMAG at1%); "i"
Print "  expected      = 0.5 + 0.5i"
Print

' =============================================
' Test 4: C_ASINH directly and via round-trip
' =============================================
Print "--- C_ASINH ---"
Dim sh% = Math(C_SINH z%)
Print "  sinh(z) = "; Math(C_REAL sh%); " + "; Math(C_IMAG sh%); "i"
Dim ash1% = Math(C_ASINH sh%)
Print "  asinh(sinh(z)) = "; Math(C_REAL ash1%); " + "; Math(C_IMAG ash1%); "i"
Print "  expected        = 0.5 + 0.5i"
Print

' Also test asinh(0) = 0
Dim z0% = Math(C_CPLX 0.0, 0.0)
Dim ash0% = Math(C_ASINH z0%)
Print "  asinh(0+0i)  = "; Math(C_REAL ash0%); " + "; Math(C_IMAG ash0%); "i"
Print "  expected      = 0 + 0i"
Print

' =============================================
' Test 5: C_ACOSH directly and via round-trip
' =============================================
Print "--- C_ACOSH ---"
Dim ch% = Math(C_COSH z%)
Print "  cosh(z) = "; Math(C_REAL ch%); " + "; Math(C_IMAG ch%); "i"
Dim ach1% = Math(C_ACOSH ch%)
Print "  acosh(cosh(z)) = "; Math(C_REAL ach1%); " + "; Math(C_IMAG ach1%); "i"
Print "  expected        = 0.5 + 0.5i"
Print

' Also test acosh(1) = 0
Dim z1% = Math(C_CPLX 1.0, 0.0)
Dim ach0% = Math(C_ACOSH z1%)
Print "  acosh(1+0i)  = "; Math(C_REAL ach0%); " + "; Math(C_IMAG ach0%); "i"
Print "  expected      = 0 + 0i"
Print

' =============================================
' Test 6: Direct inverse function with known values
' Manually provide the exact input to each inverse function
' sin(0.5+0.5i) = sin(0.5)*cosh(0.5) + i*cos(0.5)*sinh(0.5)
' =============================================
Print "--- Direct inverse calls with computed inputs ---"
Dim sr = Sin(0.5)*((Exp(0.5)+Exp(-0.5))/2)
Dim si = Cos(0.5)*((Exp(0.5)-Exp(-0.5))/2)
Print "  Computed sin(0.5+0.5i) = "; sr; " + "; si; "i"

Dim zsin_known% = Math(C_CPLX sr, si)
Dim as_known% = Math(C_ASIN zsin_known%)
Print "  asin(above) = "; Math(C_REAL as_known%); " + "; Math(C_IMAG as_known%); "i"
Print "  expected    = 0.5 + 0.5i"
Print

Print "============================================="
Print " If values above show 0+0i instead of 0.5+0.5i,"
Print " then the Pico SDK C library functions"
Print " (casinf/catanf/casinhf/cacoshf) have bugs."
Print "============================================="
End
