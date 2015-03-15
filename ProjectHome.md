<h1>PROJET - Service de répertoire partagé POSIX</h1>

<h2><br />

Objectif</h2>

<div>

<div>L'objectif de ce projet est de proposer un service réparti de répertoire partagé dont l'implémentation respecte la norme POSIX.</div>

</div>

<div>

<h2> </h2>

</div>

<h2>Présentation</h2>

<div>Le but de ce service est de garantir que tous les programmes clients accèdent aux mêmes données de manière cohérente.<br />

Chaque processus client démarre avec un répertoire local identique en tous points à celui des autres clients : mêmes noms et mêmes contenus de fichiers. On veut que les clients puissent lire et modifier le contenu de n'importe quel fichier du répertoire local sans pour autant compromettre la cohérence du répertoire partagé. La difficulté réside dans le fait qu'on doit toujours accéder à la version la plus à jour d'un fichier.<br />

De fait, plusieurs clients peuvent lire un même fichier de manière concurrente. Par contre, un accès en écriture doit forcément se faire de manière exclusive.<br />

Bien évidemment, nous vous proposons de travailler sur une version très simplifiée de ce service.<br />

D'une part, la gestion de la cohérence est centralisée : un serveur s'occupera de gérer les informations concernant les requêtes de lecture et d'écriture, et de contrôler les clients qui ont en leur possession la version la plus à jour de chaque fichier.<br />

D'autre part, nous allons vous détailler le fonctionnement attendu du service.<br />

<blockquote></div></blockquote>

<br />

<h3><a></a>Accès aux fichiers du répertoire partagé</h3>

<div>Les clients peuvent accéder aux fichiers du répertoire partagé soit en lecture, soit en écriture. L'accès en écriture est exclusif, l'accès en lecture ne l'est pas.<br />

Les accès à chaque fichier sont contrôlés par le serveur. En conséquence, ce dernier gère une file d'attente par fichier.<br />

Pour pouvoir obtenir un accès, il faut passer par une fonction de verrouillage du fichier concerné. La fonction se présente sous la forme suivante :</div>

<div><span><pre><code>int lock(char* nom_fichier, int operation);</code></pre></span></div>

<div><span>avec   </span></div>

<div><span><span><pre><code>nom_fichier</code></pre></span></span><span>          </span>le nom du fichier auquel le client désire accéder</div>

<div><span><span><pre><code>operation</code></pre></span></span><span>               </span>le type d'accès demandé (READ ou WRITE)</div>

<div><br />

Lorsqu'un client A demande le verrouillage d'un fichier, le serveur vérifie que le client possède bien une version à jour. Si ce n'est pas le cas, il lui renvoie l'adresse d'un autre client B qui possède la version la plus à jour. Le client A doit alors contacter le client B pour récupérer le contenu du fichier concerné.<br />

<blockquote><br /></blockquote>

Lorsqu'un client a fini d'accéder à un fichier, il le libère en appelant la fonction suivante :</div>

<div><pre><code>int unlock(char* nom_fichier);</code></pre></div>

<div><span>avec   </span></div>

<div><span><pre><code>nom_fichier</code></pre></span><span>          </span>le nom du fichier auquel le client désire accéder</div>

<div><br />

Le fonctionnement d'un déverrouillage diffère selon le type d'accès au fichier.<br />

Si le client accédait en écriture, alors il est considéré par le serveur comme le propriétaire de la version la plus à jour du fichier concerné. Les autres clients possèdent alors forcément une version invalide du même fichier.</div>

<br />

<h3><a></a>Programme serveur et programme client</h3>

<div>Le programme serveur est chargé de la gestion exclusive des accès aux fichiers. Les clients qui veulent accéder à un fichier appellent la fonction lock, dont le code assure le contact du serveur par TCP, et la transmission de la demande de verrouillage. Le serveur est également contacté de manière implicite lors d'un appel à unlock.<br />

Le programme serveur est lancé ainsi :<br />

<div><span><pre><code>$ shared_directory_server </code></pre></span><i><span><pre><code>&lt;port&gt; &lt;config_file&gt;</code></pre></span></i></div>

<span>avec   </span><br />

<div><span><pre><code>port</code></pre></span><span>                            le </span><span>numéro du </span><span>port sur lequel le serveur attend les demandes de connexion<br />

</span><pre><code>config_file</code></pre>        le nom d'un fichier de configuration : ce fichier liste le contenu du répertoire partagé, avec tous les noms des fichiers qui s'y trouvent.</div>

<blockquote><br /></blockquote>

Le programme client est lancé ainsi :<br />

<div><span><pre><code>$ shared_directory_client </code></pre><i><pre><code>&lt;addr&gt; &lt;port&gt; &lt;dir_path&gt;</code></pre></i></span></div>

<span>avec   </span><br />

<div><span><pre><code>addr</code></pre></span><span>             l'adresse à laquelle le serveur attend les demandes de connexion</span><br />

<span><pre><code>port</code></pre></span><span>             le numéro du port sur lequel le récepteur attend les demandes de connexion<br />

<pre><code>dir_path</code></pre>    le chemin de la copie locale du répert</span>oire</div>

<span>  </span><br />

Les méta-données gérées sur le serveur pour contrôler les accès et guarantir la cohérence du répertoire partagé sont laissées à votre jugement ; idem pour le format des messages échangés entre les clients et le serveur.<br />

Chaque choix de conception que vous ferez doit toutefois donner lieu à une justification de votre part dans le <span>rapport</span> que vous soumettrez en même temps que votre implémentation. Pour plus de détails sur le rapport, voyez la <a>partie "Travail à remettre"</a>.</div>

<br />

<div> </div>

<h3><a></a>Travail à remettre</h3>

<div>On attend de vous que vous rendiez à la fois du code et un rapport.<br />

<h4>Code</h4>

<div>Le code devra être rendu selon le format de soumission qui vous a été décrit dès le TME1.<br />

Il comportera en particulier une directive de compilation nommée sharedrep qui permettra de générer vos programmes prêts à être exécutés.<br />

Il comprendra également un programme client présentant un exemple d'utilisation.</div>

<h4>Rapport</h4>

<br />

<div>Le rapport (20 pages max au format PDF uniquement) doit contenir :</div>

<ol>

<blockquote><li>La description de la problématique (ie. l'explication, en utilisant vos propres termes, du travail qui vous est demandé.)</li></blockquote>

<blockquote><li>L'analyse du problème (cette partie doit faire ressortir <span>en français et sans la moindre ligne de code</span> vos choix de conceptions, les problèmes que vous vous attendez à rencontrer et la manière dont vous comptez les résoudre.)</li></blockquote>

<blockquote><li>L'architecture de votre solution (la description, avec les structures de données et les variables que vous allez utiliser, avec des diagrammes pour représenter les relations entre les variables et entre les processus, du fonctionnement de chaque opération dans votre bibliothèque ; prenez soin de bien détailler les variables, leur type et leur utilité.)</li></blockquote>

<blockquote><li>Un mini-manuel utilisateur (pour chaque opération, la spécification du comportement à attendre lorsqu'on l'appelle.)</li></blockquote>

</ol>

<div><br />

<span>Le rapport ne doit pas contenir : votre code</span>. Celui-ci n'a pas d'intérêt à part celui de pouvoir être compilé puis exécuté.</div>

<br />

<h4>Rappel des points importants</h4>

<div>Vous êtes essentiellement évalués en fonction de votre rapport. Travaillez d'abord sur vos choix de conception et documentez-les correctement. Ensuite prévoyez les structures de données et les variables que vous allez utiliser ; tout ceci aussi, documentez-le correctement. Alors, et seulement alors, faites une implémentation la plus simple possible. Le code ne représente qu'un quart de la note finale : il vaut mieux un rapport bien fait qui explique pourquoi l'application plante, plutôt qu'une application qui marche parfaitement sans qu'on comprenne pourquoi... Finalement, une fois que votre implémentation est finie, préparez un manuel qui permette à l'utilisateur lambda d'utiliser votre service (en partant du principe que lambda n'est pas très malin, le pôvre.)<br />

Le travail en binômes est autorisé ; par contre <span>aucun groupe de plus de 2 personnes ne sera accepté</span>.<br />

Vous devez rendre votre travail (code en archive gzippée + rapport au format PDF) pour le 3 janvier 2011 à minuit dernier délai.</div>

</div>