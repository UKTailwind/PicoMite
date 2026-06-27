from fpdf import FPDF

OUTPUT = r'd:\Dropbox\PicoMite\PicoMite\docs\GPS_Astro_Reference.pdf'

# ---------------------------------------------------------------------------
# PDF subclass: page header and footer
# ---------------------------------------------------------------------------
class DocPDF(FPDF):
    def header(self):
        self.set_font('Arial', 'B', 14)
        self.cell(0, 10, 'GPS and Astronomical Commands Reference', 0, 1, 'C')
        self.ln(2)

    def footer(self):
        self.set_y(-15)
        self.set_font('Arial', 'I', 8)
        self.cell(0, 10, 'Page ' + str(self.page_no()), 0, 0, 'C')

# ---------------------------------------------------------------------------
# Helper writers
# ---------------------------------------------------------------------------
pdf = DocPDF()
pdf.set_margins(20, 25, 20)
pdf.set_auto_page_break(True, margin=25)
pdf.add_page()

def h1(text):
    pdf.ln(4)
    pdf.set_font('Arial', 'B', 12)
    pdf.multi_cell(0, 7, text)
    pdf.ln(1)

def h2(text):
    pdf.ln(3)
    pdf.set_font('Arial', 'B', 11)
    pdf.multi_cell(0, 6, text)
    pdf.ln(1)

def h3(text):
    pdf.ln(2)
    pdf.set_font('Arial', 'B', 10)
    pdf.multi_cell(0, 5, text)

def body(text):
    pdf.set_font('Arial', '', 10)
    pdf.multi_cell(0, 5, text)
    pdf.ln(1)

def code(text):
    pdf.set_fill_color(235, 235, 235)
    pdf.set_font('Courier', '', 9)
    pdf.multi_cell(0, 5, text, 0, 'L', True)
    pdf.set_fill_color(255, 255, 255)
    pdf.ln(1)

# ---------------------------------------------------------------------------
# Section 1 – Overview
# ---------------------------------------------------------------------------
h1('1. Overview')
body('The functionality is split into four commands:')
body('- STAR: Calculates the position of a celestial object using the current time and location'
     ' from the connected GPS module.')
body('- LOCATION: Sets a manual location (latitude/longitude) and time for use with the ASTRO command.')
body('- ASTRO: Calculates the position of a celestial object using the manual time and location'
     ' set by the LOCATION command.')
body('- SLEW: Computes the motor deltas required to point a polar-aligned German Equatorial Mount'
     ' (GEM) at a target, automatically detecting and resolving meridian flips.')
body('Both STAR and ASTRO share the same syntax for specifying the target object and output variables.')

# ---------------------------------------------------------------------------
# Section 2 – Command Syntax
# ---------------------------------------------------------------------------
h1('2. Command Syntax')

# 2.1 STAR
h2('2.1 The STAR Command')
body('Uses live GPS data. Requires a valid GPS fix.')
h3('Syntax 1: Named Object')
code('STAR object   alt, az [, ra_out, dec_out]\n'
     'STAR "name"   alt, az [, ra_out, dec_out]\n'
     'STAR name$    alt, az [, ra_out, dec_out]')
body('The object name can be specified in three ways:')
body('- Plain keyword (no quotes, space separator):  STAR MOON alt, az')
body('- String literal (quoted, space or comma separator):      STAR "Aldebaran", alt, az or STAR "Aldebaran" alt, az')
body('- String variable (space or comma separator):             STAR s$, alt, az  or  STAR s$ alt, az'
     '  where s is declared DIM s AS STRING')
body('Parameters:')
body('- object / "name" / name$: The celestial body to locate. See Appendix A for the full catalog.')
body('- alt: Variable to store the calculated Altitude (degrees above horizon).')
body('- az: Variable to store the calculated Azimuth (degrees, 0 = North, 90 = East).')
body("- ra_out (Optional): Variable to store the object's current Right Ascension (hours).")
body("- dec_out (Optional): Variable to store the object's current Declination (degrees).")
h3('Examples')
code(
    "' Plain keyword -- space separator\n"
    "STAR SATURN alt, az\n"
    "\n"
    "' String literal -- comma separator\n"
    'STAR "Saturn", alt, az, ra, dec\n'
    "\n"
    "' String variable -- comma separator\n"
    "DIM target$ = \"Saturn\"\n"
    "STAR target$, alt, az\n"
    "\n"
    "' String variable declared without $\n"
    "DIM target AS STRING\n"
    'target = "Saturn"\n'
    "STAR target, alt, az"
)
h3('Syntax 2: Manual Coordinates')
code('STAR alt, az, ra, dec [, pm_ra, pm_dec] [, ra_out, dec_out]')
body('- alt, az: Variables to store the results.')
body('- ra: Right Ascension of the target in J2000.0 epoch (hours).')
body('- dec: Declination of the target in J2000.0 epoch (degrees).')
body('- pm_ra (Optional): Proper motion in RA (arcseconds/year). Default is 0.')
body('- pm_dec (Optional): Proper motion in Dec (arcseconds/year). Default is 0.')
body('- ra_out, dec_out (Optional): Variables to store the coordinates after precession to the'
     ' current epoch.')

# 2.2 LOCATION
h2('2.2 The LOCATION Command')
body('Sets the context for the ASTRO and SLEW commands.')
h3('Syntax')
code('LOCATION date$, lat, long [, sidereal_out]')
body('- date$: A string containing the date and time in the format "dd/mm/yyyy hh:mm:ss".'
     ' The separators can be -, /, :, or space.')
body('- lat: Latitude in degrees (negative for South).')
body('- long: Longitude in degrees (negative for West).')
body('- sidereal_out (Optional): Variable to store the calculated Local Sidereal Time (hours).')
h3('Example')
code('LOCATION "25/12/2025 22:30:00", -33.86, 151.21, lst')

# 2.3 ASTRO
h2('2.3 The ASTRO Command')
body('Identical to STAR but uses the context set by LOCATION instead of the GPS.')
h3('Syntax')
code('ASTRO object     alt, az [, ra_out, dec_out]\n'
     'ASTRO "name"   , alt, az [, ra_out, dec_out]\n'
     'ASTRO name$    , alt, az [, ra_out, dec_out]\n'
     'ASTRO alt, az, ra, dec [, pm_ra, pm_dec] [, ra_out, dec_out]')
body('The object name accepts the same three forms as STAR: a plain keyword, a quoted string'
     ' literal, or a string variable (with or without $ suffix). When a string literal or'
     ' variable is used a comma is required between the name and the output variables.')

# 2.4 SLEW
h2('2.4 The SLEW Command')
body('Computes the RA and Dec motor deltas needed to slew a polar-aligned GEM from its current'
     ' pointing position to a target obtained from STAR or ASTRO. Automatically detects whether'
     ' a meridian flip is required and returns safe flip directions that keep the counterweight'
     ' down and the tube above the horizon throughout the manoeuvre.')

h3('Syntax')
code('SLEW dRA1, dDec1, flipRA, flipDec, dRA2, dDec2, mountRA, mountDec, RAs, DECs, LST')

h3('Output Parameters')
body('- dRA1 (hours, -12..+12): First RA move. Positive = slew east. Always populated.')
body('- dDec1 (degrees): First Dec move. Positive = slew north. Always populated.')
body('- flipRA (hours): RA motor flip movement. +12 = east-to-west flip, -12 = west-to-east'
     ' flip, 0 = no flip required.')
body('- flipDec (degrees): Dec motor flip movement. +180 = east-to-west flip, -180 = west-to-east'
     ' flip, 0 = no flip required.')
body('- dRA2 (hours, -12..+12): Second RA move after flip. 0 if no flip required.')
body('- dDec2 (degrees): Second Dec move after flip. Always 0 - Dec is pre-positioned in move 1.')

h3('Input Parameters')
body('- mountRA (hours, 0-24): Sky RA the mount is currently pointing at.')
body('- mountDec (degrees): Sky Dec the mount is currently pointing at.')
body('- RAs (hours, 0-24): Target Right Ascension from STAR or ASTRO.')
body('- DECs (degrees): Target Declination from STAR or ASTRO.')
body('- LST (hours, 0-24): Local Sidereal Time from LOCATION.')

h3('Meridian Flip Behaviour')
body('A flip is required when the target is on the opposite side of the meridian from the current'
     ' mount position. This is detected by comparing the signs of the current and target Hour'
     ' Angles (HA = LST - RA; negative = east of meridian, positive = west).')
body('When a flip is required the caller must execute three steps:')
body('1. Apply dRA1 / dDec1: slews to the meridian and pre-positions Dec to the target value.')
body('2. Apply flipRA / flipDec: the physical flip. The RA motor rotates 12 h (180 deg) to carry'
     ' the tube to the other side of the pier; the Dec motor rotates 180 deg to re-acquire the'
     ' target Dec. Directions are chosen to swing the OTA over the top through the polar direction,'
     ' keeping the counterweight down and the tube above the horizon.')
body('3. Apply dRA2: fine RA correction from the meridian to the target. Dec needs no correction'
     ' because the flip preserves sky Dec exactly.')
body('When no flip is required, flipRA, flipDec, dRA2, and dDec2 are all 0.')

h3('Example')
code(
    "LOCATION \"07/05/2026 21:30:00\", 51.5, -1.8, lst\n"
    "mount_ra  = lst   ' home: on the meridian\n"
    "mount_dec = 90.0  ' OTA pointing at celestial pole\n"
    "\n"
    "ASTRO Saturn alt, az, ra, dec\n"
    "IF alt < 5 THEN PRINT \"Target below horizon\" : END\n"
    "\n"
    "SLEW dra1, ddec1, fra, fdec, dra2, ddec2, mount_ra, mount_dec, ra, dec, lst\n"
    "\n"
    "drive_RA(dra1) : drive_Dec(ddec1)     ' move 1\n"
    "\n"
    "IF fra <> 0 THEN\n"
    "  drive_RA(fra)     ' +12 or -12 hours: swings tube over the top\n"
    "  drive_Dec(fdec)   ' +180 or -180 degrees: re-acquires Dec\n"
    "  drive_RA(dra2)    ' fine RA correction to target\n"
    "END IF\n"
    "\n"
    "mount_ra = ra : mount_dec = dec"
)

h3('Notes')
body('- All RA parameters (dRA1, dRA2, flipRA, mountRA, RAs, LST) are in hours.'
     ' Multiply by 15 to convert to degrees for stepper motor calculations.')
body('- Dec parameters are in degrees.')
body('- The calling program must track mount_ra and mount_dec, updating them after each'
     ' successful slew so that subsequent SLEW calls compute correct deltas.')
body('- Obtain LST from LOCATION before calling SLEW; LST changes continuously so it should'
     ' be refreshed before each slew.')
body('- Check alt > 0 from STAR/ASTRO before calling SLEW to confirm the target is above'
     ' the horizon.')
body('- Dec motor direction is physically reversed after a meridian flip. The sign of flipDec'
     ' (+180 or -180) encodes the correct direction; pass it directly to the motor driver'
     ' without modification.')

# ---------------------------------------------------------------------------
# Section 3 – Implementation Details
# ---------------------------------------------------------------------------
h1('3. Implementation Details & Math')
body('The implementation performs high-precision astronomical calculations suitable for'
     ' telescope pointing or navigation.')

h2('3.1 Time Systems')
body('- Julian Centuries (T): Time is converted to Julian Centuries from the J2000.0 epoch'
     ' (2000 Jan 1.5 TT).')
body('- Sidereal Time: Local Sidereal Time (LST) is calculated using a polynomial approximation'
     " for Greenwich Mean Sidereal Time (GMST) and the observer's longitude.")

h2('3.2 Planetary Calculations')
body('- Planets: Uses simplified VSOP87-based elements with perturbations for Mercury, Venus,'
     ' Mars, Jupiter, Saturn, Uranus, and Neptune.')
body('- Moon: Uses a truncated ELP-2000/82 analytical series to calculate the Moon\'s position,'
     ' including significant periodic terms for longitude, latitude, and distance.')
body('- Topocentric Correction: For the Moon and planets, the calculation corrects for the'
     " observer's position on the Earth's surface (parallax), which is critical for nearby"
     ' bodies like the Moon.')

h2('3.3 Stellar Calculations')
body('- Catalog: Contains J2000.0 coordinates and proper motion data for bright stars and deep'
     ' sky objects.')
body('- Precession: Coordinates are precessed from the J2000.0 epoch to the current date using'
     ' the rigorous method described in Meeus (Chapter 21).')
body('- Proper Motion: Applied based on the years elapsed since J2000.0.')

h2('3.4 Coordinate Conversion')
body('- Equatorial to Horizontal: Converts Right Ascension and Declination to Altitude and'
     ' Azimuth using standard spherical trigonometry.')
body('- Refraction: Atmospheric refraction is applied to the Altitude using the standard formula'
     ' (Meeus Chapter 16), corrected for standard atmospheric pressure and temperature.')

# ---------------------------------------------------------------------------
# Appendix A – Celestial Catalog
# ---------------------------------------------------------------------------
pdf.add_page()
h1('Appendix A: Celestial Catalog')
body('The following objects are recognized by name in the STAR and ASTRO commands.')

h3('Solar System Bodies:')
pdf.set_font('Arial', '', 10)
pdf.multi_cell(0, 5, 'SUN, MOON, MERCURY, VENUS, MARS, JUPITER, SATURN, URANUS, NEPTUNE')
pdf.ln(3)

h3('Stars and Deep Sky Objects:')
pdf.ln(2)

stars = [
    'Achernar',           'Acrux',                 'Alcyone',
    'Aldebaran',          'Algenib',               'Algieba',
    'Algol',              'Alhajoth',              'Alhena',
    'Almaak',             'Alnair',                'Alnilam',
    'Alnitak',            'Alphard',               'Alpheratz',
    'Alpherg',            'Alrescha',              'Alsephina',
    'Alshain',            'Altair',                'Aludra',
    'Andromeda Galaxy',   'Antares',               'Arcturus',
    'Aspidiske',          'Bellatrix',             'Betelgeuse',
    'Bodes Galaxy',       'Canopus',               'Capella',
    'Caph',               'Castor',                'Cigar Galaxy',
    'Deneb',              'Denebola',              'Dubhe',
    'Elnath',             'Eltanin',               'Enif',
    'Fomalhaut',          'Gacrux',                'Hadar',
    'Homam',              'Kaus Australis',        'Kochab',
    'Kornephoros',        'Large Magellanic Cloud','Lesath',
    'Markab',             'Menkalinan',            'Mimosa',
    'Mintaka',            'Mirfak',                'Nunki',
    'Peacock',            'Polaris',               'Pollux',
    'Procyon',            'Rasalgethi',            'Rasalhague',
    'Regulus',            'Rigel',                 'Rigil Kent',
    'Ruchbah',            'Sabik',                 'Sadalmelik',
    'Sadalsuud',          'Sadr',                  'Saiph',
    'Scheat',             'Shaula',                'Shedir',
    'Sirius',             'Small Magellanic Cloud','Sombrero Galaxy',
    'Spica',              'Suhail',                'Tarazed',
    'Triangulum Galaxy',  'Vega',                  'Whirlpool Galaxy',
    'Zubenelgenubi',      'Zubeneschamali',
]

col_w = (pdf.w - pdf.l_margin - pdf.r_margin) / 3
pdf.set_font('Courier', '', 9)
i = 0
while i < len(stars):
    row = stars[i:i+3]
    for j, name in enumerate(row):
        pdf.set_x(pdf.l_margin + j * col_w)
        ln_val = 1 if j == 2 else 0
        pdf.cell(col_w, 5, name, 0, ln_val)
    if len(row) < 3:
        pdf.ln(5)
    i += 3

# ---------------------------------------------------------------------------
# Output
# ---------------------------------------------------------------------------
pdf.output(OUTPUT, 'F')
print('Written: ' + OUTPUT)
