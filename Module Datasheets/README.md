# Module component datasheets

| File | Component | Source | Used by |
|---|---|---|---|
| [VEML6040_Vishay_84276.pdf](VEML6040_Vishay_84276.pdf) | Vishay VEML6040A3OG, Rev. 1.7 (28-Nov-2024), document 84276 | [Manufacturer datasheet](https://www.vishay.com/docs/84276/veml6040.pdf) | Colour sensor |
| [XL-5050RGBC-2812B-S_C22461793.pdf](XL-5050RGBC-2812B-S_C22461793.pdf) | XINGLIGHT XL-5050RGBC-WS2812B-S (PDF title), LCSC C22461793 | [Owner-provided LCSC datasheet](https://www.lcsc.com/datasheet/C22461793.pdf) | LED matrix |
| [AT8833CT_C575404.pdf](AT8833CT_C575404.pdf) | AT8833 / AT8833CT, AT8833-S402-V1.4 English document | [Distributor-hosted document](https://www.icyi.com/storage/uploads/mp4/202505/01/1746029314_UsKhlXC7PL.pdf) | Stepper motor |
| [223_C1693666.pdf](223_C1693666.pdf) | XMXW 223, LCSC C1693666, Revision 1.1 | [Owner-provided LCSC datasheet](https://www.lcsc.com/datasheet/C1693666.pdf) | Touch sensor |

The touch-controller file was downloaded from the PDF URL exposed by the
owner-provided LCSC page on 2026-10-24. Use this exact part's mode and timing
tables; a TTP223 datasheet is not interchangeable evidence for this XMXW part.
Original datasheets are retained unchanged. Add exact component references
here as further modules are implemented.

The VEML6040 manufacturer PDF was downloaded unchanged on 2026-10-26.
Register words and repeated-start read requirements are on pages 7–9.

The LED PDF was downloaded unchanged on 2026-10-25. Its title includes
WS2812B-S; the saved filename follows the LCSC 2812B-S listing. Use this exact
part's timing table/notes and GRB order rather than another WS2812 variant.


TFT manufacturer manual, ILI9341 datasheet, example archives and initialization
source are linked with reviewed SHA-256 hashes in
[the TFT reference notes](../py32f040_TFT_Module/ThirdParty/README.md). Those
reference PDFs/archives are not redistributed here. The adapted initialization
source retains the manufacturer's MIT license in that project.


The AT8833 document was downloaded unchanged on2026-10-28 from the linked
English distributor mirror. Its printed revision is AT8833-S402-V1.4, with 22
PDF pages (printed page numbering differs). The exact BOM part is AT8833CT,
LCSC C575404; [LCSC reference](https://www.lcsc.com/datasheet/C575404.pdf) currently
returns a JavaScript viewer and its raw PDF endpoint denied this download.
Use the saved document's ETSSOP16 pin table, bridge truth table, current-trip
specification and wake/fault behavior; do not substitute a DRV8833 document.
Motor source: [NFP-15BY product page](https://dcmotorpro.com/product/15mm-micro-dc-stepper-motor-11mm-type-model-nfp-15by).
The owner selected the A variant (1.5 mm shaft,10 ohm); B has different leads
and resistance. The product page is linked rather than redistributed.
