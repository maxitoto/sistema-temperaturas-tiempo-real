declare module "exceljs/dist/exceljs.min.js" {
  import type * as ExcelJSType from "exceljs";
  const ExcelJSBrowser: typeof ExcelJSType;
  export default ExcelJSBrowser;
}