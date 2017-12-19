final case class LDAResults(
  name: String,
  docTopicProbs: Array[Array[Double]],
  topicWordProbs: Array[Array[Double]],
  docNames: Array[String],
  vocab: Array[String]
) {
  val numDocs = docTopicProbs.size
  val numTopics = docTopicProbs(0).size
  val numWords = topicWordProbs(0).size

  def sortedDocTopicProbsAndIdx(docIdx: Int): Array[(Double, Int)] = {
    val probs = docTopicProbs(docIdx)
    probs.zipWithIndex.sortBy(_._1)(Ordering[Double].reverse)
  }

  def sortedDocTopicsProbsAndWords(docIdx: Int): Array[(Double, Array[(Double, String)])] = {
    sortedDocTopicProbsAndIdx(docIdx) map { case (prob, topicIdx) =>
      (prob, sortedTopicWordProbsAndWord(topicIdx))
    }
  }

  def sortedTopicWordProbsAndIdx(topicIdx: Int): Array[(Double, Int)] = {
    val probs = topicWordProbs(topicIdx)
    probs.zipWithIndex.sortBy(_._1)(Ordering[Double].reverse)
  }

  def sortedTopicWordProbsAndWord(topicIdx: Int): Array[(Double, String)] = {
    sortedTopicWordProbsAndIdx(topicIdx) map { case (prob, idx) =>
      (prob, vocab(idx))
    }
  }

  def sortedTopicDocProbsAndIdx(topicIdx: Int): Array[(Double, Int)] = {
    val probs = docTopicProbs map { _(topicIdx) }
    probs.zipWithIndex.sortBy(_._1)(Ordering[Double].reverse)
  }

  def sortedTopicDocProbsAndName(topicIdx: Int): Array[(Double, String)] = {
    sortedTopicDocProbsAndIdx(topicIdx) map { case (prob, docIdx) =>
      (prob, docNames(docIdx))
    }
  }

  def sortedWordTopicProbsAndIdx(wordIdx: Int): Array[(Double, Int)] = {
    val probs = topicWordProbs map { _(wordIdx) }
    probs.zipWithIndex.sortBy(_._1)(Ordering[Double].reverse)
  }

  def sortedWordTopicProbsAndTopicWords(wordIdx: Int): Array[(Double, Array[(Double, String)])] = {
    sortedWordTopicProbsAndIdx(wordIdx) map { case (prob, topicIdx) =>
      (prob, sortedTopicWordProbsAndWord(topicIdx))
    }
  }

  def sortedWordIdxsByTopicProb: Array[Int] = {
    (0 until numWords).toArray.sortBy { wordIdx =>
      sortedWordTopicProbsAndIdx(wordIdx)(0)._1
    }(Ordering.Double.reverse)
  }

  def print(): Unit = {
    println(name)
    println("sortedTopicWordProbsAndWord")
    println("----------------------------")
    for (topicIdx <- 0 until numTopics) {
      println(s"Topic $topicIdx")
      sortedTopicWordProbsAndWord(topicIdx).take(5) map { case (prob, word) =>
        val probStr = "%.4f".format(prob)
        println(s"$word ($probStr)")
      }
    }
    println("\n")
    println("sortedTopicDocProbsAndName")
    println("----------------------------")
    for (topicIdx <- 0 until numTopics) {
      println(s"Topic $topicIdx")
      sortedTopicDocProbsAndName(topicIdx).take(5) map { case (prob, docName) =>
        val probStr = "%.4f".format(prob)
        println(s"$docName ($probStr)")
      }
    }
    println("\n")
    println("sortedWordTopicProbsAndIdx")
    println("----------------------------")
    for (wordIdx <- sortedWordIdxsByTopicProb) {
      println(s"Word '${vocab(wordIdx)}'")
      sortedWordTopicProbsAndTopicWords(wordIdx).take(5) map { case (prob, topicWords) =>
        println(s"${topicWords.take(5).mkString(",")} ($prob)")
      }
    }
    println("\n\n")
  }
}

object ExtractInterestingLDADimensions {

  val AllVersions = Seq(
    ("10 topics, ignore counts", "10x1"),
    ("15 topics, ignore counts", "15x1"),
    ("20 topics, ignore counts", "20x1"),
    ("10 topics, tf-idf", "10x1v2"),
    ("15 topics, tf-idf", "15x1v2"),
    ("20 topics, tf-idf", "20x1v2"),
    ("10 topics, original matrix", "10x1v3"),
    ("15 topics, original matrix", "15x1v3"),
    ("20 topics, original matrix", "20x1v3")
  )

  def mkDataPath(name: String): String = {
    "/Users/tstramer/workspace/source/topics/data/" + name
  }

  def loadLines(name: String): Array[String] = {
    scala.io.Source.fromFile(mkDataPath(name)).getLines.toArray
  }

  def loadMatrix(name: String): Array[Array[Double]] = {
    loadLines(name).toArray map { line =>
      line.split(",").toArray.map(_.toDouble)
    }
  }

  lazy val vocab: Array[String] = loadLines("vocab")

  lazy val docNames: Array[String] = loadLines("usersToQueryCounts") map { line =>
    line.split("\t")(0)
  }

  def loadLDAResults(name: String, suffix: String): LDAResults = {
    LDAResults(
      name = name,
      docTopicProbs = loadMatrix("docTopicProbs" + suffix),
      topicWordProbs = loadMatrix("topicWordProbs" + suffix),
      docNames = docNames,
      vocab = vocab
    )
  }

  def loadAllLDAResults: Seq[LDAResults] = {
    AllVersions map { case (name, suffix) =>
      loadLDAResults(name, suffix)
    }
  }
}
