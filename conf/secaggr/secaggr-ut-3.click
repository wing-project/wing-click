
as :: AvailableSensors();

s1 :: Sensor(ID 1, SENSORS as, VALUE 10, DEBUG false, SAMPLE 1);
s2 :: Sensor(ID 2, SENSORS as, VALUE 10, DEBUG false, SAMPLE 1);
s3 :: Sensor(ID 3, SENSORS as, VALUE 10, DEBUG false, SAMPLE 1);

ap :: AggregationPoint(SENSORS as);

s1 -> [0] ap;
s2 -> [1] ap;
s3 -> [2] ap;

index :: AvailableSensorsIndex(as);
hbh :: HBHAggregationPoint(INDEX index);

ap -> hbh;

hbh
 -> Unqueue()
 -> Sink(SENSORS as, DEBUG true)
 -> Discard();

