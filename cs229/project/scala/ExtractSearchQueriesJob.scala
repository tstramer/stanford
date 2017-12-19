import com.google.common.annotations.VisibleForTesting
import com.twitter.clientapp.thriftscala.{EventDetails, LogEvent}
import com.twitter.loggedout.analytics.common.{EventNamespaceWrapper, LOLogEventFilters, LogEventWrapper}
import com.twitter.pluck.source.client_event.scrooge.ClientEventSourceScrooge
import com.twitter.scalding.TDsl._
import com.twitter.scalding._
import com.twitter.scalding.source.TypedText
import com.twitter.scalding_internal.job.{RequiredBinaryComparators, TwitterDateJob}
import com.twitter.search.common.scalding.search_client_events.source_scrooge.SearchClientEventSourceScrooge
import scala.util.Random

/**

Run:
  ./pants compile topics && ./pants bundle topics:topics-deploy
  oscar hdfs --screen \
    --hadoop-client-memory 4096 \
    --bundle topics-deploy --job ExtractSearchQueriesJob \
    -- \
    --date 2015-11-01T12 2015-11-07T12 \
    --output searchQueries

To see the data:
  science/scalding/scripts/hdfsget searchQueries
  cat searchQueries
  */
class ExtractSearchQueriesJob(args : Args) extends TwitterDateJob(args) with RequiredBinaryComparators {

  import ExtractSearchQueriesJob._

  val clientEvents: TypedSource[LogEvent] = SearchClientEventSourceScrooge()(dateRange)

  clientEvents
    .flatMap(extractSearchQueryFromLogEvent)
    .map { query => (query, 1) }
    .sumByKey
    .toTypedPipe
    .filter { case (_, count) => count >= MinSearches }
    .write(TypedText.tsv[(String, Int)](args("output")))
}

object ExtractSearchQueriesJob {

  private val MinSearches = 2500

  def extractSearchQueryFromLogEvent(logEvent: LogEvent): Option[String] = {
    for {
      logEventWrapper <- LogEventWrapper(logEvent)
      language <- logEventWrapper.logBase.language
      if isSearchEvent(logEventWrapper.eventNamespace) &&
        language.equalsIgnoreCase("en") &&
        isGoodLogEvent(logEventWrapper)
      query <- logEvent.eventDetails.flatMap(extractSearchQuery)
    } yield query.toLowerCase
  }

  def isSearchEvent(eventNamespaceWrapper: EventNamespaceWrapper): Boolean =
    eventNamespaceWrapper.action.equalsIgnoreCase("search")

  def isGoodLogEvent(logEventWrapper: LogEventWrapper): Boolean =
    !LOLogEventFilters.isBotUserAgent(logEventWrapper) &&
      LOLogEventFilters.isValidGuestID(logEventWrapper) &&
      (logEventWrapper.guestCookieAgeBucket != 0 || logEventWrapper.userId.isDefined)

  def extractSearchQuery(eventDetails: EventDetails): Option[String] =
    for {
      items <- eventDetails.items
      item <- items.headOption
      queryDetails <- item.queryDetails
      query <- queryDetails.itemQuery
    } yield query
}