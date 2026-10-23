export const developerServices = {
    logDebugInfo: function (info: string): void {
        console.log(info);
    },
    
    echo: async (msg: string): Promise<string> => {
        const sleep = (ms: number) => new Promise<void>(r => setTimeout(r, ms));
        await sleep(5000);
        return msg;
    }
}