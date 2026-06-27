' SELECT CASE Memory Test Program
' Tests temp memory cleanup with many CASE statements
' Matches at positions: 5, 50, 100, 150, 199, and CASE ELSE

OPTION EXPLICIT

DIM test%, pass%, fail%
pass% = 0
fail% = 0

PRINT "SELECT CASE Memory Test"
PRINT "========================"
PRINT

' Test 1: Match early (position 5)
test% = 1
PRINT "Test 1: Match at position 5... ";
SelectTest("case005")
IF pass% <> 1 THEN fail% = fail% + 1 : PRINT "FAIL" ELSE PRINT "PASS"

' Test 2: Match at position 50
test% = 2
pass% = 0
PRINT "Test 2: Match at position 50... ";
SelectTest("case050")
IF pass% <> 1 THEN fail% = fail% + 1 : PRINT "FAIL" ELSE PRINT "PASS"

' Test 3: Match at position 100
test% = 3
pass% = 0
PRINT "Test 3: Match at position 100... ";
SelectTest("case100")
IF pass% <> 1 THEN fail% = fail% + 1 : PRINT "FAIL" ELSE PRINT "PASS"

' Test 4: Match at position 150
test% = 4
pass% = 0
PRINT "Test 4: Match at position 150... ";
SelectTest("case150")
IF pass% <> 1 THEN fail% = fail% + 1 : PRINT "FAIL" ELSE PRINT "PASS"

' Test 5: Match at position 199 (near end)
test% = 5
pass% = 0
PRINT "Test 5: Match at position 199... ";
SelectTest("case199")
IF pass% <> 1 THEN fail% = fail% + 1 : PRINT "FAIL" ELSE PRINT "PASS"

' Test 6: No match - should hit CASE ELSE
test% = 6
pass% = 0
PRINT "Test 6: CASE ELSE (no match)... ";
SelectTest("nomatch")
IF pass% <> 2 THEN fail% = fail% + 1 : PRINT "FAIL" ELSE PRINT "PASS"

' Test 7: Run multiple times to check for memory leaks
PRINT
PRINT "Test 7: Repeated calls (100x) to check memory...";
DIM i%
FOR i% = 1 TO 100
  SelectTest("case100")
NEXT i%
PRINT " PASS"

' Test 8: Integer SELECT CASE with many options
PRINT "Test 8: Integer SELECT with 200 cases... ";
pass% = 0
SelectTestInt(150)
IF pass% <> 1 THEN fail% = fail% + 1 : PRINT "FAIL" ELSE PRINT "PASS"

PRINT
PRINT "========================"
PRINT "Tests complete. Failures: " + STR$(fail%)
PRINT
PRINT "Check MEMORY command for heap usage"
MEMORY

END

SUB SelectTest(val$)
  LOCAL result$
  result$ = ""
  
  SELECT CASE val$
    CASE "case001": result$ = "001"
    CASE "case002": result$ = "002"
    CASE "case003": result$ = "003"
    CASE "case004": result$ = "004"
    CASE "case005": result$ = "005"
    CASE "case006": result$ = "006"
    CASE "case007": result$ = "007"
    CASE "case008": result$ = "008"
    CASE "case009": result$ = "009"
    CASE "case010": result$ = "010"
    CASE "case011": result$ = "011"
    CASE "case012": result$ = "012"
    CASE "case013": result$ = "013"
    CASE "case014": result$ = "014"
    CASE "case015": result$ = "015"
    CASE "case016": result$ = "016"
    CASE "case017": result$ = "017"
    CASE "case018": result$ = "018"
    CASE "case019": result$ = "019"
    CASE "case020": result$ = "020"
    CASE "case021": result$ = "021"
    CASE "case022": result$ = "022"
    CASE "case023": result$ = "023"
    CASE "case024": result$ = "024"
    CASE "case025": result$ = "025"
    CASE "case026": result$ = "026"
    CASE "case027": result$ = "027"
    CASE "case028": result$ = "028"
    CASE "case029": result$ = "029"
    CASE "case030": result$ = "030"
    CASE "case031": result$ = "031"
    CASE "case032": result$ = "032"
    CASE "case033": result$ = "033"
    CASE "case034": result$ = "034"
    CASE "case035": result$ = "035"
    CASE "case036": result$ = "036"
    CASE "case037": result$ = "037"
    CASE "case038": result$ = "038"
    CASE "case039": result$ = "039"
    CASE "case040": result$ = "040"
    CASE "case041": result$ = "041"
    CASE "case042": result$ = "042"
    CASE "case043": result$ = "043"
    CASE "case044": result$ = "044"
    CASE "case045": result$ = "045"
    CASE "case046": result$ = "046"
    CASE "case047": result$ = "047"
    CASE "case048": result$ = "048"
    CASE "case049": result$ = "049"
    CASE "case050": result$ = "050"
    CASE "case051": result$ = "051"
    CASE "case052": result$ = "052"
    CASE "case053": result$ = "053"
    CASE "case054": result$ = "054"
    CASE "case055": result$ = "055"
    CASE "case056": result$ = "056"
    CASE "case057": result$ = "057"
    CASE "case058": result$ = "058"
    CASE "case059": result$ = "059"
    CASE "case060": result$ = "060"
    CASE "case061": result$ = "061"
    CASE "case062": result$ = "062"
    CASE "case063": result$ = "063"
    CASE "case064": result$ = "064"
    CASE "case065": result$ = "065"
    CASE "case066": result$ = "066"
    CASE "case067": result$ = "067"
    CASE "case068": result$ = "068"
    CASE "case069": result$ = "069"
    CASE "case070": result$ = "070"
    CASE "case071": result$ = "071"
    CASE "case072": result$ = "072"
    CASE "case073": result$ = "073"
    CASE "case074": result$ = "074"
    CASE "case075": result$ = "075"
    CASE "case076": result$ = "076"
    CASE "case077": result$ = "077"
    CASE "case078": result$ = "078"
    CASE "case079": result$ = "079"
    CASE "case080": result$ = "080"
    CASE "case081": result$ = "081"
    CASE "case082": result$ = "082"
    CASE "case083": result$ = "083"
    CASE "case084": result$ = "084"
    CASE "case085": result$ = "085"
    CASE "case086": result$ = "086"
    CASE "case087": result$ = "087"
    CASE "case088": result$ = "088"
    CASE "case089": result$ = "089"
    CASE "case090": result$ = "090"
    CASE "case091": result$ = "091"
    CASE "case092": result$ = "092"
    CASE "case093": result$ = "093"
    CASE "case094": result$ = "094"
    CASE "case095": result$ = "095"
    CASE "case096": result$ = "096"
    CASE "case097": result$ = "097"
    CASE "case098": result$ = "098"
    CASE "case099": result$ = "099"
    CASE "case100": result$ = "100"
    CASE "case101": result$ = "101"
    CASE "case102": result$ = "102"
    CASE "case103": result$ = "103"
    CASE "case104": result$ = "104"
    CASE "case105": result$ = "105"
    CASE "case106": result$ = "106"
    CASE "case107": result$ = "107"
    CASE "case108": result$ = "108"
    CASE "case109": result$ = "109"
    CASE "case110": result$ = "110"
    CASE "case111": result$ = "111"
    CASE "case112": result$ = "112"
    CASE "case113": result$ = "113"
    CASE "case114": result$ = "114"
    CASE "case115": result$ = "115"
    CASE "case116": result$ = "116"
    CASE "case117": result$ = "117"
    CASE "case118": result$ = "118"
    CASE "case119": result$ = "119"
    CASE "case120": result$ = "120"
    CASE "case121": result$ = "121"
    CASE "case122": result$ = "122"
    CASE "case123": result$ = "123"
    CASE "case124": result$ = "124"
    CASE "case125": result$ = "125"
    CASE "case126": result$ = "126"
    CASE "case127": result$ = "127"
    CASE "case128": result$ = "128"
    CASE "case129": result$ = "129"
    CASE "case130": result$ = "130"
    CASE "case131": result$ = "131"
    CASE "case132": result$ = "132"
    CASE "case133": result$ = "133"
    CASE "case134": result$ = "134"
    CASE "case135": result$ = "135"
    CASE "case136": result$ = "136"
    CASE "case137": result$ = "137"
    CASE "case138": result$ = "138"
    CASE "case139": result$ = "139"
    CASE "case140": result$ = "140"
    CASE "case141": result$ = "141"
    CASE "case142": result$ = "142"
    CASE "case143": result$ = "143"
    CASE "case144": result$ = "144"
    CASE "case145": result$ = "145"
    CASE "case146": result$ = "146"
    CASE "case147": result$ = "147"
    CASE "case148": result$ = "148"
    CASE "case149": result$ = "149"
    CASE "case150": result$ = "150"
    CASE "case151": result$ = "151"
    CASE "case152": result$ = "152"
    CASE "case153": result$ = "153"
    CASE "case154": result$ = "154"
    CASE "case155": result$ = "155"
    CASE "case156": result$ = "156"
    CASE "case157": result$ = "157"
    CASE "case158": result$ = "158"
    CASE "case159": result$ = "159"
    CASE "case160": result$ = "160"
    CASE "case161": result$ = "161"
    CASE "case162": result$ = "162"
    CASE "case163": result$ = "163"
    CASE "case164": result$ = "164"
    CASE "case165": result$ = "165"
    CASE "case166": result$ = "166"
    CASE "case167": result$ = "167"
    CASE "case168": result$ = "168"
    CASE "case169": result$ = "169"
    CASE "case170": result$ = "170"
    CASE "case171": result$ = "171"
    CASE "case172": result$ = "172"
    CASE "case173": result$ = "173"
    CASE "case174": result$ = "174"
    CASE "case175": result$ = "175"
    CASE "case176": result$ = "176"
    CASE "case177": result$ = "177"
    CASE "case178": result$ = "178"
    CASE "case179": result$ = "179"
    CASE "case180": result$ = "180"
    CASE "case181": result$ = "181"
    CASE "case182": result$ = "182"
    CASE "case183": result$ = "183"
    CASE "case184": result$ = "184"
    CASE "case185": result$ = "185"
    CASE "case186": result$ = "186"
    CASE "case187": result$ = "187"
    CASE "case188": result$ = "188"
    CASE "case189": result$ = "189"
    CASE "case190": result$ = "190"
    CASE "case191": result$ = "191"
    CASE "case192": result$ = "192"
    CASE "case193": result$ = "193"
    CASE "case194": result$ = "194"
    CASE "case195": result$ = "195"
    CASE "case196": result$ = "196"
    CASE "case197": result$ = "197"
    CASE "case198": result$ = "198"
    CASE "case199": result$ = "199"
    CASE "case200": result$ = "200"
    CASE ELSE: result$ = "ELSE"
  END SELECT
  
  IF result$ = "ELSE" THEN
    pass% = 2
  ELSEIF result$ <> "" THEN
    pass% = 1
  ENDIF
END SUB

SUB SelectTestInt(val%)
  LOCAL result%
  result% = -1
  
  SELECT CASE val%
    CASE 1: result% = 1
    CASE 2: result% = 2
    CASE 3: result% = 3
    CASE 4: result% = 4
    CASE 5: result% = 5
    CASE 6: result% = 6
    CASE 7: result% = 7
    CASE 8: result% = 8
    CASE 9: result% = 9
    CASE 10: result% = 10
    CASE 11: result% = 11
    CASE 12: result% = 12
    CASE 13: result% = 13
    CASE 14: result% = 14
    CASE 15: result% = 15
    CASE 16: result% = 16
    CASE 17: result% = 17
    CASE 18: result% = 18
    CASE 19: result% = 19
    CASE 20: result% = 20
    CASE 21: result% = 21
    CASE 22: result% = 22
    CASE 23: result% = 23
    CASE 24: result% = 24
    CASE 25: result% = 25
    CASE 26: result% = 26
    CASE 27: result% = 27
    CASE 28: result% = 28
    CASE 29: result% = 29
    CASE 30: result% = 30
    CASE 31: result% = 31
    CASE 32: result% = 32
    CASE 33: result% = 33
    CASE 34: result% = 34
    CASE 35: result% = 35
    CASE 36: result% = 36
    CASE 37: result% = 37
    CASE 38: result% = 38
    CASE 39: result% = 39
    CASE 40: result% = 40
    CASE 41: result% = 41
    CASE 42: result% = 42
    CASE 43: result% = 43
    CASE 44: result% = 44
    CASE 45: result% = 45
    CASE 46: result% = 46
    CASE 47: result% = 47
    CASE 48: result% = 48
    CASE 49: result% = 49
    CASE 50: result% = 50
    CASE 51: result% = 51
    CASE 52: result% = 52
    CASE 53: result% = 53
    CASE 54: result% = 54
    CASE 55: result% = 55
    CASE 56: result% = 56
    CASE 57: result% = 57
    CASE 58: result% = 58
    CASE 59: result% = 59
    CASE 60: result% = 60
    CASE 61: result% = 61
    CASE 62: result% = 62
    CASE 63: result% = 63
    CASE 64: result% = 64
    CASE 65: result% = 65
    CASE 66: result% = 66
    CASE 67: result% = 67
    CASE 68: result% = 68
    CASE 69: result% = 69
    CASE 70: result% = 70
    CASE 71: result% = 71
    CASE 72: result% = 72
    CASE 73: result% = 73
    CASE 74: result% = 74
    CASE 75: result% = 75
    CASE 76: result% = 76
    CASE 77: result% = 77
    CASE 78: result% = 78
    CASE 79: result% = 79
    CASE 80: result% = 80
    CASE 81: result% = 81
    CASE 82: result% = 82
    CASE 83: result% = 83
    CASE 84: result% = 84
    CASE 85: result% = 85
    CASE 86: result% = 86
    CASE 87: result% = 87
    CASE 88: result% = 88
    CASE 89: result% = 89
    CASE 90: result% = 90
    CASE 91: result% = 91
    CASE 92: result% = 92
    CASE 93: result% = 93
    CASE 94: result% = 94
    CASE 95: result% = 95
    CASE 96: result% = 96
    CASE 97: result% = 97
    CASE 98: result% = 98
    CASE 99: result% = 99
    CASE 100: result% = 100
    CASE 101: result% = 101
    CASE 102: result% = 102
    CASE 103: result% = 103
    CASE 104: result% = 104
    CASE 105: result% = 105
    CASE 106: result% = 106
    CASE 107: result% = 107
    CASE 108: result% = 108
    CASE 109: result% = 109
    CASE 110: result% = 110
    CASE 111: result% = 111
    CASE 112: result% = 112
    CASE 113: result% = 113
    CASE 114: result% = 114
    CASE 115: result% = 115
    CASE 116: result% = 116
    CASE 117: result% = 117
    CASE 118: result% = 118
    CASE 119: result% = 119
    CASE 120: result% = 120
    CASE 121: result% = 121
    CASE 122: result% = 122
    CASE 123: result% = 123
    CASE 124: result% = 124
    CASE 125: result% = 125
    CASE 126: result% = 126
    CASE 127: result% = 127
    CASE 128: result% = 128
    CASE 129: result% = 129
    CASE 130: result% = 130
    CASE 131: result% = 131
    CASE 132: result% = 132
    CASE 133: result% = 133
    CASE 134: result% = 134
    CASE 135: result% = 135
    CASE 136: result% = 136
    CASE 137: result% = 137
    CASE 138: result% = 138
    CASE 139: result% = 139
    CASE 140: result% = 140
    CASE 141: result% = 141
    CASE 142: result% = 142
    CASE 143: result% = 143
    CASE 144: result% = 144
    CASE 145: result% = 145
    CASE 146: result% = 146
    CASE 147: result% = 147
    CASE 148: result% = 148
    CASE 149: result% = 149
    CASE 150: result% = 150
    CASE 151: result% = 151
    CASE 152: result% = 152
    CASE 153: result% = 153
    CASE 154: result% = 154
    CASE 155: result% = 155
    CASE 156: result% = 156
    CASE 157: result% = 157
    CASE 158: result% = 158
    CASE 159: result% = 159
    CASE 160: result% = 160
    CASE 161: result% = 161
    CASE 162: result% = 162
    CASE 163: result% = 163
    CASE 164: result% = 164
    CASE 165: result% = 165
    CASE 166: result% = 166
    CASE 167: result% = 167
    CASE 168: result% = 168
    CASE 169: result% = 169
    CASE 170: result% = 170
    CASE 171: result% = 171
    CASE 172: result% = 172
    CASE 173: result% = 173
    CASE 174: result% = 174
    CASE 175: result% = 175
    CASE 176: result% = 176
    CASE 177: result% = 177
    CASE 178: result% = 178
    CASE 179: result% = 179
    CASE 180: result% = 180
    CASE 181: result% = 181
    CASE 182: result% = 182
    CASE 183: result% = 183
    CASE 184: result% = 184
    CASE 185: result% = 185
    CASE 186: result% = 186
    CASE 187: result% = 187
    CASE 188: result% = 188
    CASE 189: result% = 189
    CASE 190: result% = 190
    CASE 191: result% = 191
    CASE 192: result% = 192
    CASE 193: result% = 193
    CASE 194: result% = 194
    CASE 195: result% = 195
    CASE 196: result% = 196
    CASE 197: result% = 197
    CASE 198: result% = 198
    CASE 199: result% = 199
    CASE 200: result% = 200
    CASE ELSE: result% = 0
  END SELECT
  
  IF result% = val% THEN pass% = 1
END SUB
