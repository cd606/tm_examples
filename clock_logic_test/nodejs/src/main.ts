import * as TMInfra from '@dev_cd606/tm_infra'
import * as TMBasic from '@dev_cd606/tm_basic'
import * as util from 'util'

class Facility extends TMInfra.RealTimeApp.OnOrderFacility<any, string, string> {
    private logic(queryKey: string, dataInput: string): string {
        return "Reply to '" + queryKey + "' is '" + dataInput + "'";
    }
    private outsideInput: string | null;
    private queryInput: TMInfra.Key<string>[];
    public constructor() {
        super();
        this.outsideInput = null;
        this.queryInput = [];
    }
    public start(_e: any): void { }
    public setOutsideInput(s: TMInfra.TimedDataWithEnvironment<any, string>) {
        this.outsideInput = s.timedData.value;
        for (let q of this.queryInput) {
            this.publish({
                environment: s.environment
                , timedData: {
                    timePoint: s.timedData.timePoint
                    , value: {
                        id: q.id
                        , key: this.logic(q.key, s.timedData.value)
                    }
                    , finalFlag: true
                }
            });
        }
        this.queryInput = [];
    }
    public handle(d: TMInfra.TimedDataWithEnvironment<any, TMInfra.Key<string>>): void {
        if (this.outsideInput == null) {
            this.queryInput.push(d.timedData.value);
        } else {
            this.publish({
                environment: d.environment
                , timedData: {
                    timePoint: d.timedData.timePoint
                    , value: {
                        id: d.timedData.value.id
                        , key: this.logic(d.timedData.value.key, this.outsideInput)
                    }
                    , finalFlag: true
                }
            });
        }
    }
}

async function main() {
    type MyEnvironment = TMBasic.ClockEnv;
    let env: MyEnvironment = new TMBasic.ClockEnv(
        TMBasic.ClockEnv.clockSettingsWithStartPointCorrespondingToNextAlignment(
            1
            , new Date("2020-01-01T10:00:00")
            , 2.0
        )
    );
    let r = new TMInfra.RealTimeApp.Runner<MyEnvironment>(env);

    let importer1 = TMBasic.ClockImporter.createRecurringClockImporter<MyEnvironment, string>(
        new Date("2020-01-01T10:00:00.121")
        , new Date("2020-01-01T10:01:00.012")
        , 5000
        , (d: Date) => "RECURRING " + TMBasic.ClockEnv.formatDate(d)
    );
    let importer2 = TMBasic.ClockImporter.createRecurringClockImporter<MyEnvironment, string>(
        new Date("2020-01-01T10:00:12")
        , new Date("2020-01-01T10:00:28")
        , 16000
        , (d: Date) => "RECURRING 2 " + TMBasic.ClockEnv.formatDate(d)
    );
    let importer3 = TMBasic.ClockImporter.createOneShotClockImporter<MyEnvironment, string>(
        new Date("2020-01-01T10:00:27")
        , (d: Date) => "ONESHOT " + TMBasic.ClockEnv.formatDate(d)
    );
    let converter = TMInfra.RealTimeApp.Utils.liftPure(
        (s: string) => TMInfra.keyify(s)
    );
    let facility = new Facility();
    let exporter = TMInfra.RealTimeApp.Utils.pureExporter(
        (s: string) => {
            env.log(TMInfra.LogLevel.Info, s);
        }
    );
    let exporter2 = TMInfra.RealTimeApp.Utils.pureExporter(
        (s: TMInfra.KeyedData<string, string>) => {
            env.log(TMInfra.LogLevel.Info, util.inspect(s));
        }
    );
    let clockFacility = TMBasic.ClockOnOrderFacility.createClockCallback(
        (d: Date, thisIdx: number, total: number) =>
            `CALLBACK (${thisIdx} out of ${total}) ${TMBasic.ClockEnv.formatDate(d)}`
    );
    let clockFacilityInput = TMInfra.RealTimeApp.Utils.liftPure(
        function (s: string): TMInfra.Key<TMBasic.ClockOnOrderFacilityInput<string>> {
            return TMInfra.keyify({
                inputData: s
                , callbackDurations: [150, 1000, 5250]
            });
        }
    );
    let clockFacilityOutput = TMInfra.RealTimeApp.Utils.liftPure(
        (d: TMInfra.KeyedData<TMBasic.ClockOnOrderFacilityInput<string>, string>) =>
            d.key.key.inputData + " --- " + d.data
    );
    let feedItemToFacility = TMInfra.RealTimeApp.Utils.simpleExporter(
        (s: TMInfra.TimedDataWithEnvironment<MyEnvironment, string>) => {
            facility.setOutsideInput(s);
        }
    );

    r.exportItem(exporter, r.importItem(importer1));
    r.placeOrderWithFacility(
        r.execute(converter, r.importItem(importer2))
        , facility
        , r.exporterAsSink(exporter2)
    );
    r.placeOrderWithFacility(
        r.execute(clockFacilityInput, r.importItem(importer1))
        , clockFacility
        , r.actionAsSink(clockFacilityOutput)
    );
    r.exportItem(feedItemToFacility, r.importItem(importer3));
    r.exportItem(exporter, r.actionAsSource(clockFacilityOutput));

    r.finalize();
}

main().catch(err => {
    console.error(err);
    process.exit(1);
});