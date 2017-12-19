scala_library(
  name = 'topics',
  dependencies = [
    '3rdparty/jvm/com/twitter/scalding:args',
    '3rdparty/jvm/com/twitter/scalding:core',
    '3rdparty/jvm/com/twitter/scalding:commons',
    'science/src/protobuf/com/twitter/twadoop', #Contains Status protobuf
    'science/src/scala/com/twitter/pluck/extractor:twitter-extractors',
    'science/src/scala/com/twitter/pluck/source/combined_user_source',
    'science/src/scala/com/twitter/pluck/source/combined_user_scrooge_source:combined_user_scrooge_source',
    'science/src/scala/com/twitter/pluck/source/verified_profile_request_daily_source',
    'science/src/scala/com/twitter/pluck/source/client_event',
    'science/src/scala/com/twitter/pluck/source/client_event/scrooge',
    'science/src/scala/com/twitter/pluck/source:remote-sources',
    'science/src/scala/com/twitter/roastduck/util',
    'science/src/scala/com/twitter/scalding_internal/job',
    'science/src/java/com/twitter/search/common/util/text:text',
    'science/src/scala/com/twitter/loggedout/analytics/common',
    'science/src/thrift/com/twitter/clientapp/gen:clientapp-scala',
    'science/src/scala/com/twitter/search/common/scalding/search_client_events/source_scrooge',
  ],
  sources=globs("*.scala")
)

UNUSED_EXCLUDES = [
  exclude('ant', 'ant'),
  exclude('com.sun.jdmk', 'jmxtools'),
  exclude('com.sun.jmx', 'jmxri'),
  exclude('hsqldb'),
  exclude('javax.jms', 'jms'),
  exclude('javax.mail', 'mail'),
  #needed for log4j
  exclude('org.codehaus.janino', 'janino'),
  exclude('org.apache.hive'),
  exclude('org.apache.pig'),
  exclude('org.eclipse.jdt'),
  #hadoop requires this, but we don't call it and should be on the cluster.
  exclude('tomcat'),
]

hadoop_binary(
  name = "topics-deploy",
  main = "com.twitter.scalding.Tool",
  dependencies = [
    ':topics',
  ],
  excludes = UNUSED_EXCLUDES,
)

jvm_binary(
  name = "topics-local",
  main = "com.twitter.scalding.Tool",
  dependencies = [
    ':topics',
  ],
  excludes = UNUSED_EXCLUDES,
)